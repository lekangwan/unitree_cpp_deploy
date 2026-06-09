#include "FSM/CtrlFSM.h"
#include "FSM/State_Passive.h"
#include "FSM/State_FixStand.h"
#include "FSM/State_RLBase.h"

#include <rclcpp/rclcpp.hpp>
#include "terrain_msgs/msg/terrain_result.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <thread>

std::unique_ptr<LowCmd_t> FSMState::lowcmd = nullptr;
std::shared_ptr<LowState_t> FSMState::lowstate = nullptr;
std::shared_ptr<Keyboard> FSMState::keyboard = nullptr;

std::string terrainToFSMState(int32_t class_id)
{
    switch (class_id)
    {
    case 0:                     // 普通地形
    case 1:                     // 海绵垫
    case 4:                     // 斜坡
        return "Velocity_Down"; // MoE

    case 2:                        // 亚克力
        return "Velocity_SlipDog"; // SlipDog

    case 3:                      // 楼梯
        return "Velocity_Right"; // HIMLOCO

    default:
        return "";
    }
}

void print_keyboard_help()
{
    std::cout << "\n--- Keyboard Controls ---\n";
    std::cout << "  [1] Passive        [2] FixStand\n";
    std::cout << "  [3] CTS            [4] MoE-CTS\n";
    std::cout << "  [5] WalkTheseWay   [6] HIMLOCO\n";
    std::cout << "  [7] SlipDog        [8] DreamWaQ\n";
    std::cout << "  W/↑: Forward    S/↓: Backward\n";
    std::cout << "  A/←: Strafe Left    D/→: Strafe Right\n";
    std::cout << "  Q/RotL    E/RotR    Space: Stop\n";
    std::cout << "-------------------------\n"
              << std::endl;
}

void init_fsm_state()
{
    auto lowcmd_sub = std::make_shared<unitree::robot::go2::subscription::LowCmd>();
    usleep(0.2 * 1e6);
    if (!lowcmd_sub->isTimeout())
    {
        spdlog::critical("The other process is using the lowcmd channel, please close it first.");
        unitree::robot::go2::shutdown();
        // exit(0);
    }
    FSMState::lowcmd = std::make_unique<LowCmd_t>();
    FSMState::lowstate = std::make_shared<LowState_t>();
    spdlog::info("Waiting for connection to robot...");
    FSMState::lowstate->wait_for_connection();
    spdlog::info("Connected to robot.");
}

int main(int argc, char **argv)
{
    // Load parameters
    auto vm = param::helper(argc, argv);

    std::cout << " --- Unitree Robotics --- \n";
    std::cout << "     Go2 Controller \n";

    // Unitree DDS Config
    unitree::robot::ChannelFactory::Instance()->Init(0, vm["network"].as<std::string>());

    init_fsm_state();

    // Initialize keyboard
    FSMState::keyboard = std::make_shared<Keyboard>();

    // Initialize FSM from config
    auto fsm = std::make_unique<CtrlFSM>(param::config["FSM"]);
    fsm->start();

    // ROS2 terrain subscriber
    rclcpp::init(argc, argv);

    auto terrain_node = std::make_shared<rclcpp::Node>("terrain_policy_selector");

    auto last_target_state = std::make_shared<std::string>("");
    auto last_switch_time = std::make_shared<std::chrono::steady_clock::time_point>(
        std::chrono::steady_clock::now());

    auto terrain_sub = terrain_node->create_subscription<terrain_msgs::msg::TerrainResult>(
        "/terrain/result", // 改成你 terrain_node.py 实际发布的话题名
        10,
        [fsm_ptr = fsm.get(), last_target_state, last_switch_time](const terrain_msgs::msg::TerrainResult::SharedPtr msg)
        {
            // 置信度过低就不切换，阈值可以按实际效果调整
            if (msg->similarity < 0.60f)
            {
                return;
            }

            std::string target_state = terrainToFSMState(msg->class_id);
            if (target_state.empty())
            {
                spdlog::warn("Unknown terrain class_id: {}", msg->class_id);
                return;
            }

            auto now = std::chrono::steady_clock::now();

            // 防抖：同一个目标状态 1 秒内不要重复触发
            if (*last_target_state == target_state &&
                now - *last_switch_time < std::chrono::seconds(1))
            {
                return;
            }

            *last_target_state = target_state;
            *last_switch_time = now;

            spdlog::info(
                "Terrain detected: id={}, name={}, similarity={:.3f}, switch to {}",
                msg->class_id,
                msg->class_name,
                msg->similarity,
                target_state);

            fsm_ptr->requestState(target_state);
        });

    std::thread ros_thread([terrain_node]()
                           { rclcpp::spin(terrain_node); });
    ros_thread.detach();

    std::cout << "Press [L2 + A] to enter FixStand mode.\n";
    std::cout << "Then press [Start + Up/Down/Left/Right] to select and start a policy.\n";
    std::cout << "Or use keyboard [1-8] to switch states and WASD/arrows to drive.\n";
    print_keyboard_help();

    while (true)
    {
        sleep(1);
    }

    return 0;
}
