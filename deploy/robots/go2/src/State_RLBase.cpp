#include "FSM/State_RLBase.h"
#include "unitree_articulation.h"
#include "isaaclab/envs/mdp/observations/observations.h"
#include "isaaclab/envs/mdp/actions/joint_actions.h"

State_RLBase::State_RLBase(int state_mode, std::string state_string)
: FSMState(state_mode, state_string) 
{
    auto cfg = param::config["FSM"][state_string];
    
    // Check if policy_dir exists and is not null
    if (!cfg["policy_dir"] || cfg["policy_dir"].IsNull()) {
        spdlog::warn("State_{}: Policy key 'policy_dir' is null or undefined. This state will be disabled.", state_string);
        return;
    }

    auto policy_dir = param::parser_policy_dir(cfg["policy_dir"].as<std::string>());

    env = std::make_unique<isaaclab::ManagerBasedRLEnv>(
        YAML::LoadFile(policy_dir / "params" / "deploy.yaml"),
        std::make_shared<unitree::BaseArticulation<LowState_t::SharedPtr>>(FSMState::lowstate)
    );
    env->alg = std::make_unique<isaaclab::OrtRunner>(policy_dir / "exported" / "policy.onnx");

    this->registered_checks.emplace_back(
        std::make_pair(
            [&]()->bool{ return isaaclab::mdp::bad_orientation(env.get(), 2.0); },
            FSMStringMap.right.at("Passive")
        )
    );

    // Initialize logger
    if (cfg["logging"] && cfg["logging"].as<bool>()) {
        enable_logging = true;
        if (cfg["logging_dt"]) {
            logging_dt = std::chrono::duration<double>(cfg["logging_dt"].as<double>());
        }
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");
        std::string filename = "run_data_" + ss.str() + ".csv";
        auto logs_dir = policy_dir / "logs";
        if (!std::filesystem::exists(logs_dir)) {
            std::filesystem::create_directories(logs_dir);
        }
        auto file_path = (logs_dir / filename).string();
        logger = std::make_unique<DataLogger>(file_path);
        spdlog::info("Logging enabled. Saving to {}", file_path);
        
        start_time = std::chrono::steady_clock::now();
        last_log_time = start_time - std::chrono::duration_cast<std::chrono::steady_clock::duration>(logging_dt);
    }

    // Initialize fixed command settings
    if (cfg["fixed_command"] && cfg["fixed_command"]["enabled"]) {
        env->fixed_command_enabled = cfg["fixed_command"]["enabled"].as<bool>();
        if (env->fixed_command_enabled) {
            env->fixed_lin_vel_x = cfg["fixed_command"]["lin_vel_x"].as<float>();
            env->fixed_lin_vel_y = cfg["fixed_command"]["lin_vel_y"].as<float>();
            env->fixed_ang_vel_z = cfg["fixed_command"]["ang_vel_z"].as<float>();
            if (cfg["fixed_command"]["duration"]) {
                env->fixed_command_duration = cfg["fixed_command"]["duration"].as<float>();
            }
            spdlog::info("Fixed command enabled: lin_vel_x={:.2f}, lin_vel_y={:.2f}, ang_vel_z={:.2f}, duration={:.1f}s",
                env->fixed_lin_vel_x, env->fixed_lin_vel_y, env->fixed_ang_vel_z, env->fixed_command_duration);
            spdlog::info("Press [L2 + Y] to toggle fixed command execution");
        }
    }

    // Initialize gait parameters (for WalkTheseWay / models with 15d commands)
    // Read from deploy.yaml (env->cfg), not config.yaml
    if (env && env->cfg["gait_params"]) {
        auto gp = env->cfg["gait_params"];
        if (gp["gait_frequency"]) env->gait_frequency = gp["gait_frequency"].as<float>();
        if (gp["footswing_height"]) env->footswing_height = gp["footswing_height"].as<float>();
        if (gp["body_height"]) env->body_height = gp["body_height"].as<float>();
        if (gp["body_pitch"]) env->body_pitch = gp["body_pitch"].as<float>();
        if (gp["body_roll"]) env->body_roll = gp["body_roll"].as<float>();
        if (gp["stance_width"]) env->stance_width = gp["stance_width"].as<float>();
        if (gp["stance_length"]) env->stance_length = gp["stance_length"].as<float>();
        if (gp["gait_duration"]) env->gait_duration = gp["gait_duration"].as<float>();
        if (gp["gait_phase_fl"]) env->gait_phase_fl = gp["gait_phase_fl"].as<float>();
        if (gp["gait_phase_fr"]) env->gait_phase_fr = gp["gait_phase_fr"].as<float>();
        if (gp["gait_phase_rl"]) env->gait_phase_rl = gp["gait_phase_rl"].as<float>();
        if (gp["aux_reward"]) env->aux_reward = gp["aux_reward"].as<float>();
        spdlog::info("Gait params: freq={:.1f}Hz, footswing={:.3f}m, body_h={:.3f}, pitch={:.2f}, roll={:.2f}",
            env->gait_frequency, env->footswing_height, env->body_height, env->body_pitch, env->body_roll);
    }
}

void State_RLBase::run()
{
    if (!env) return;

    // Check for L2 + Y to toggle fixed command execution
    if (env->fixed_command_enabled) {
        auto & joy = lowstate->joystick;
        if (joy.LT.pressed && joy.Y.on_pressed) {
            env->fixed_command_active = !env->fixed_command_active;
            if (env->fixed_command_active) {
                env->fixed_command_start_time = std::chrono::steady_clock::now();
                if (env->fixed_command_duration > 0) {
                    spdlog::info("Fixed command ACTIVATED for {:.1f}s: lin_vel_x={:.2f}, lin_vel_y={:.2f}, ang_vel_z={:.2f}",
                        env->fixed_command_duration, env->fixed_lin_vel_x, env->fixed_lin_vel_y, env->fixed_ang_vel_z);
                } else {
                    spdlog::info("Fixed command ACTIVATED (indefinite): lin_vel_x={:.2f}, lin_vel_y={:.2f}, ang_vel_z={:.2f}",
                        env->fixed_lin_vel_x, env->fixed_lin_vel_y, env->fixed_ang_vel_z);
                }
            } else {
                spdlog::info("Fixed command DEACTIVATED, returning to joystick control");
            }
        }

        // Check duration timeout (not applied during keyboard override)
        if (env->fixed_command_active && env->fixed_command_duration > 0 && !keyboard_override_) {
            auto elapsed = std::chrono::steady_clock::now() - env->fixed_command_start_time;
            float elapsed_sec = std::chrono::duration<float>(elapsed).count();
            if (elapsed_sec >= env->fixed_command_duration) {
                env->fixed_command_active = false;
                spdlog::info("Fixed command COMPLETED after {:.1f}s, returning to joystick control", elapsed_sec);
            }
        }
    }

    // Keyboard motion control (WASD / Arrow keys)
    if (FSMState::keyboard) {
        std::string key = FSMState::keyboard->key();

        if (key == "w" || key == "up" || key == "s" || key == "down" ||
            key == "a" || key == "left" || key == "d" || key == "right" ||
            key == "q" || key == "e" || key == " ")
        {
            keyboard_override_ = true;

            float vx = 0, vy = 0, wz = 0;
            if (key == "w" || key == "up")        vx = 0.8f;
            else if (key == "s" || key == "down") vx = -0.5f;
            else if (key == "a" || key == "left") vy = 0.5f;
            else if (key == "d" || key == "right")vy = -0.5f;
            else if (key == "q")                  wz = 0.5f;
            else if (key == "e")                  wz = -0.5f;

            env->fixed_lin_vel_x = vx;
            env->fixed_lin_vel_y = vy;
            env->fixed_ang_vel_z = wz;
            env->fixed_command_active = true;
            env->fixed_command_enabled = true;
        }
        else if (keyboard_override_ && key.empty())
        {
            // Key released after keyboard was used — stop
            env->fixed_lin_vel_x = 0;
            env->fixed_lin_vel_y = 0;
            env->fixed_ang_vel_z = 0;
            env->fixed_command_active = true;
        }
    }

    auto action = env->action_manager->processed_actions();
    for(int i(0); i < env->robot->data.joint_ids_map.size(); i++) {
        lowcmd->msg_.motor_cmd()[env->robot->data.joint_ids_map[i]].q() = action[i];
    }

    // Logging
    if (enable_logging && logger) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_log_time >= logging_dt) {
            last_log_time = now;
            
            std::chrono::duration<double> time_since_start = now - start_time;
            logger->add("time", time_since_start.count());
            
            auto system_now = std::chrono::system_clock::now();
            auto duration = system_now.time_since_epoch();
            double unix_time = std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();
            std::stringstream ss_unix;
            ss_unix << std::fixed << std::setprecision(2) << unix_time;
            logger->add("unix_time", ss_unix.str());

            std::time_t now_c = std::chrono::system_clock::to_time_t(system_now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;
            std::stringstream ss_wall;
            ss_wall << std::put_time(std::localtime(&now_c), "%H:%M:%S") << '.' << std::setw(2) << std::setfill('0') << (ms / 10);
            logger->add("wall_time", ss_wall.str());

            logger->add("q_des", action);
            std::vector<float> q, dq, tau, temp;
            for (int i(0); i < 12; ++i) {
                q.push_back(lowstate->msg_.motor_state()[i].q());
                dq.push_back(lowstate->msg_.motor_state()[i].dq());
                tau.push_back(lowstate->msg_.motor_state()[i].tau_est());
                temp.push_back(lowstate->msg_.motor_state()[i].temperature());
            }
            logger->add("q", q);
            logger->add("dq", dq);
            logger->add("tau", tau);
            logger->add("temp", temp);

            std::vector<float> rpy(3), acc(3), gyro(3);
            for (int i(0); i < 3; ++i) {
                rpy[i] = lowstate->msg_.imu_state().rpy()[i];
                acc[i] = lowstate->msg_.imu_state().accelerometer()[i];
                gyro[i] = lowstate->msg_.imu_state().gyroscope()[i];
            }
            logger->add("imu_rpy", rpy);
            logger->add("imu_acc", acc);
            logger->add("ang_vel", gyro);

            std::vector<float> foot_force(4);
            for (int i(0); i < 4; ++i) {
                foot_force[i] = lowstate->msg_.foot_force()[i];
            }
            logger->add("foot_force", foot_force);

            std::vector<float> foot_contacts(4);
            for (int i(0); i < 4; ++i) {
                foot_contacts[i] = (foot_force[i] > 10.0f) ? 1.0f : 0.0f;
            }
            logger->add("foot_contact", foot_contacts);

            if (env->last_inference_results.count("weights")) {
                logger->add("weight", env->last_inference_results["weights"]);
            }
            if (env->last_inference_results.count("latent")) {
                logger->add("latent", env->last_inference_results["latent"]);
            }

            // Joystick commands (no scaling)
            logger->add("cmd_ns_0", lowstate->joystick.ly());
            logger->add("cmd_ns_1", -lowstate->joystick.lx());
            logger->add("cmd_ns_2", -lowstate->joystick.rx());

            // Fixed command (log zeros when inactive)
            float fixed_0 = 0.0f;
            float fixed_1 = 0.0f;
            float fixed_2 = 0.0f;
            if (env->fixed_command_enabled && env->fixed_command_active) {
                fixed_0 = env->fixed_lin_vel_x;
                fixed_1 = env->fixed_lin_vel_y;
                fixed_2 = env->fixed_ang_vel_z;
            }
            logger->add("cmd_fixed_0", fixed_0);
            logger->add("cmd_fixed_1", fixed_1);
            logger->add("cmd_fixed_2", fixed_2);

            logger->write();
        }
    }
}