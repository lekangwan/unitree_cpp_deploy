#include "FSM/CtrlFSM.h"
#include "FSM/State_Passive.h"
#include "FSM/State_FixStand.h"
#include "FSM/State_RLBase.h"

std::unique_ptr<LowCmd_t> FSMState::lowcmd = nullptr;
std::shared_ptr<LowState_t> FSMState::lowstate = nullptr;
std::shared_ptr<Keyboard> FSMState::keyboard = nullptr;

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
    std::cout << "-------------------------\n" << std::endl;
}

void init_fsm_state()
{
    auto lowcmd_sub = std::make_shared<unitree::robot::go2::subscription::LowCmd>();
    usleep(0.2 * 1e6);
    if(!lowcmd_sub->isTimeout())
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

int main(int argc, char** argv)
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

