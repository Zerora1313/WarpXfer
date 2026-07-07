#include "../include/cli_parser.h"
#include "../include/transfer_manager.h"

int main(int argc, char* argv[]) {
    cli::Config config = cli::ParseArgs(argc, argv);
    if (config.mode == cli::Mode::HELP) {
        cli::PrintHelp();
        return 0;
    }

    bool success = core::TransferManager::Run(config);
    return success ? 0 : 1;
}
