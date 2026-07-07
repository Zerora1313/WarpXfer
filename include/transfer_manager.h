#ifndef TRANSFER_MANAGER_H
#define TRANSFER_MANAGER_H

#include "cli_parser.h"

namespace core {

class TransferManager {
public:
    static bool Run(const cli::Config& config);
};

} // namespace core

#endif // TRANSFER_MANAGER_H
