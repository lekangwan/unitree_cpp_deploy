// Copyright (c) 2025, Unitree Robotics Co., Ltd.
// All rights reserved.

#pragma once

#include "isaaclab/envs/manager_based_rl_env.h"

namespace isaaclab
{
namespace mdp
{

REGISTER_OBSERVATION(base_ang_vel)
{
    auto & asset = env->robot;
    auto & data = asset->data.root_ang_vel_b;
    return std::vector<float>(data.data(), data.data() + data.size());
}

REGISTER_OBSERVATION(projected_gravity)
{
    auto & asset = env->robot;
    auto & data = asset->data.projected_gravity_b;
    return std::vector<float>(data.data(), data.data() + data.size());
}

REGISTER_OBSERVATION(joint_pos)
{
    auto & asset = env->robot;
    std::vector<float> data;

    if(params["asset_cfg"]["joint_ids"])
    {
        auto joint_ids = params["asset_cfg"]["joint_ids"].as<std::vector<int>>();
        data.resize(joint_ids.size());
        for(size_t i = 0; i < joint_ids.size(); ++i)
        {
            data[i] = asset->data.joint_pos[joint_ids[i]];
        }
    }
    else
    {
        data.resize(asset->data.joint_pos.size());
        for(size_t i = 0; i < asset->data.joint_pos.size(); ++i)
        {
            data[i] = asset->data.joint_pos[i];
        }
    }

    return data;
}

REGISTER_OBSERVATION(joint_pos_rel)
{
    auto & asset = env->robot;
    std::vector<float> data;

    if(params["asset_cfg"]["joint_ids"])
    {
        auto joint_ids = params["asset_cfg"]["joint_ids"].as<std::vector<int>>();
        data.resize(joint_ids.size());
        for(size_t i = 0; i < joint_ids.size(); ++i) {
            data[i] = asset->data.joint_pos[joint_ids[i]] - asset->data.default_joint_pos[joint_ids[i]];
        }
    }
    else
    {
        data.resize(asset->data.joint_pos.size());
        for(size_t i = 0; i < asset->data.joint_pos.size(); ++i) {
            data[i] = asset->data.joint_pos[i] - asset->data.default_joint_pos[i];
        }
    }

    return data;
}

REGISTER_OBSERVATION(joint_vel_rel)
{
    auto & asset = env->robot;

    if(params["asset_cfg"]["joint_ids"])
    {
        auto joint_ids = params["asset_cfg"]["joint_ids"].as<std::vector<int>>();
        std::vector<float> data(joint_ids.size());
        for(size_t i = 0; i < joint_ids.size(); ++i) {
            data[i] = asset->data.joint_vel[joint_ids[i]];
        }
        return data;
    }

    auto & data = asset->data.joint_vel;
    return std::vector<float>(data.data(), data.data() + data.size());
}

REGISTER_OBSERVATION(last_action)
{
    auto data = env->action_manager->action();
    return std::vector<float>(data.data(), data.data() + data.size());
};

REGISTER_OBSERVATION(velocity_commands)
{
    std::vector<float> obs(3);

    // Check if fixed command mode is active
    if (env->fixed_command_enabled && env->fixed_command_active) {
        obs[0] = env->fixed_lin_vel_x;
        obs[1] = env->fixed_lin_vel_y;
        obs[2] = env->fixed_ang_vel_z;
        return obs;
    }

    auto & joystick = env->robot->data.joystick;
    auto cfg = env->cfg["commands"]["base_velocity"]["ranges"];

    obs[0] = joystick->ly();
    obs[1] = -joystick->lx();
    obs[2] = -joystick->rx();

    auto scale_func = [&cfg](std::vector<float>& obs, int idx, const std::string& key) {
        if (obs[idx] > 0) {
            obs[idx] *= cfg[key][1].as<float>();
        } else {
            obs[idx] *= -cfg[key][0].as<float>();
        }
    };
    scale_func(obs, 0, "lin_vel_x");
    scale_func(obs, 1, "lin_vel_y");
    scale_func(obs, 2, "ang_vel_z");

    return obs;
}

REGISTER_OBSERVATION(gait_phase)
{
    float period = params["period"].as<float>();
    float delta_phase = env->step_dt * (1.0f / period);

    env->global_phase += delta_phase;
    env->global_phase = std::fmod(env->global_phase, 1.0f);

    std::vector<float> obs(2);
    obs[0] = std::sin(env->global_phase * 2 * M_PI);
    obs[1] = std::cos(env->global_phase * 2 * M_PI);
    return obs;
}

// ====================================================================
// WalkTheseWay 模型专用观测函数（不干扰其他模型）
// ====================================================================

REGISTER_OBSERVATION(velocity_commands_15d)
{
    std::vector<float> obs(15);

    if (env->fixed_command_enabled && env->fixed_command_active) {
        obs[0] = env->fixed_lin_vel_x;
        obs[1] = env->fixed_lin_vel_y;
        obs[2] = env->fixed_ang_vel_z;
    } else {
        auto & joy = env->robot->data.joystick;
        auto cfg = env->cfg["commands"]["base_velocity"]["ranges"];
        obs[0] = joy->ly();
        obs[1] = -joy->lx();
        obs[2] = -joy->rx();
        auto scale_func = [&cfg](std::vector<float>& o, int idx, const std::string& key) {
            if (o[idx] > 0) o[idx] *= cfg[key][1].as<float>();
            else o[idx] *= -cfg[key][0].as<float>();
        };
        scale_func(obs, 0, "lin_vel_x");
        scale_func(obs, 1, "lin_vel_y");
        scale_func(obs, 2, "ang_vel_z");
    }

    obs[3] = env->body_height;
    obs[4] = env->gait_frequency;
    obs[5] = env->gait_phase_fl;
    obs[6] = env->gait_phase_fr;
    obs[7] = env->gait_phase_rl;
    obs[8] = env->gait_duration;
    obs[9] = env->footswing_height;
    obs[10] = env->body_pitch;
    obs[11] = env->body_roll;
    obs[12] = env->stance_width;
    obs[13] = env->stance_length;
    obs[14] = env->aux_reward;
    return obs;
}

REGISTER_OBSERVATION(actions)
{
    auto data = env->action_manager->action();
    return std::vector<float>(data.data(), data.data() + data.size());
}

REGISTER_OBSERVATION(last_actions)
{
    return env->last_actions_buffer;
}

REGISTER_OBSERVATION(clock_inputs)
{
    float delta = env->step_dt * 3.0f;
    env->global_phase = std::fmod(env->global_phase + delta, 1.0f);

    float p = env->global_phase;
    std::vector<float> obs(4);
    obs[0] = std::sin(2.0f * M_PI * p);
    obs[1] = std::sin(2.0f * M_PI * std::fmod(p + 0.5f, 1.0f));
    obs[2] = std::sin(2.0f * M_PI * std::fmod(p + 0.5f, 1.0f));
    obs[3] = std::sin(2.0f * M_PI * p);
    return obs;
}

// ====================================================================
// SlipDog 模型专用观测函数
// ====================================================================

REGISTER_OBSERVATION(imu_angles)
{
    // Returns [roll, pitch] from IMU quaternion
    auto & quat = env->robot->data.root_quat_w;
    float w = quat.w(), x = quat.x(), y = quat.y(), z = quat.z();
    float roll = std::atan2(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y));
    float pitch = std::asin(2.0f * (w * y - z * x));
    return {roll, pitch};
}

REGISTER_OBSERVATION(foot_contact)
{
    // Returns foot contact state (contact - 0.5), 4 feet
    // Contact = force > 10N threshold
    std::vector<float> obs(4);
    for (int i = 0; i < 4; ++i) {
        float force = env->robot->data.foot_force[i];
        float contact = (force > 10.0f) ? 1.0f : 0.0f;
        obs[i] = contact - 0.5f;
    }
    return obs;
}

REGISTER_OBSERVATION(zero_vec_12)
{
    return std::vector<float>(12, 0.0f);
}

REGISTER_OBSERVATION(slip_dog_commands)
{
    // 7-dim commands: lin_vel_x, lin_vel_y, ang_vel_yaw, jump_height, locomotion_height, eps, class
    std::vector<float> obs(7, 0.0f);

    if (env->fixed_command_enabled && env->fixed_command_active) {
        obs[0] = env->fixed_lin_vel_x;
        obs[1] = env->fixed_lin_vel_y;
        obs[2] = env->fixed_ang_vel_z;
    } else {
        auto & joystick = env->robot->data.joystick;
        auto cfg = env->cfg["commands"]["base_velocity"]["ranges"];

        auto scale_func = [cfg](float val, const std::string& key) -> float {
            if (val > 0) return val * cfg[key][1].as<float>();
            else return val * -cfg[key][0].as<float>();
        };

        obs[0] = scale_func(joystick->ly(), "lin_vel_x");
        obs[1] = scale_func(-joystick->lx(), "lin_vel_y");
        obs[2] = scale_func(-joystick->rx(), "ang_vel_z");
    }

    // Hardcoded gait parameters for trot deployment
    obs[3] = 0.0f;   // jump_height = 0 (no jump)
    obs[4] = 0.29f;  // locomotion_height = 0.29 (default from config)
    obs[5] = 0.0f;   // latent_eps = 0
    obs[6] = 0.0f;   // latent_c = 0

    return obs;
}

}
}
