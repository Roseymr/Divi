#ifndef WALLET_BALANCE_CALCULATOR_H
#define WALLET_BALANCE_CALCULATOR_H
#include <amount.h>
class I_AppendOnlyTransactionRecord;
class I_SpentOutputTracker;
class I_MerkleTxConfirmationNumberCalculator;
class I_UtxoOwnershipDetector;
class WalletBalanceCalculator
{
private:
    const I_UtxoOwnershipDetector& ownershipDetector_;
    const I_AppendOnlyTransactionRecord& txRecord_;
    const I_SpentOutputTracker& spentOutputTracker_;
    const I_MerkleTxConfirmationNumberCalculator& confsCalculator_;

    enum BalanceFlag
    {
        UNCONFIRMED = 1 << 0,
        CONFIRMED = 1 << 1,
        TRUSTED = 1 << 2,
        TRUSTED_OR_CONFIRMED = TRUSTED | CONFIRMED,
    };
    CAmount calculateBalance(BalanceFlag flag) const;
public:
    WalletBalanceCalculator(
        const I_UtxoOwnershipDetector& ownershipDetector,
        const I_AppendOnlyTransactionRecord& txRecord,
        const I_SpentOutputTracker& spentOutputTracker,
        const I_MerkleTxConfirmationNumberCalculator& confsCalculator);
    ~WalletBalanceCalculator();
    CAmount getBalance() const;
    CAmount getUnconfirmedBalance() const;
};
#endif// WALLET_BALANCE_CALCULATOR_H