// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_H
#define BITCOIN_WALLET_H

#include <amount.h>
#include <base58address.h>
#include <pubkey.h>
#include <uint256.h>
#include <crypter.h>
#include <wallet_ismine.h>
#include <walletdb.h>
#include <ui_interface.h>
#include <FeeRate.h>
#include <boost/foreach.hpp>
#include <utilstrencodings.h>
#include <tinyformat.h>
#include <NotificationInterface.h>
#include <merkletx.h>
#include <keypool.h>
#include <reservekey.h>
#include <OutputEntry.h>
#include <Output.h>
#include <I_StakingCoinSelector.h>

class I_SignatureSizeEstimator;
class I_CoinSelectionAlgorithm;
class CKeyMetadata;
class CKey;
class CBlock;
class CScript;
class CTransaction;
class CBlockIndex;
struct StakableCoin;
class WalletTransactionRecord;
class SpentOutputTracker;
class BlockMap;
class CAccountingEntry;
class CChain;
class CCoinControl;
class COutput;
class CReserveKey;
class CScript;
class CWalletTx;
class CHDChain;
class CTxMemPool;
class CWalletDB;
class COutPoint;
class CTxIn;
class I_MerkleTxConfirmationNumberCalculator;
class I_VaultManagerDatabase;
class VaultManager;

bool IsFinalTx(const CTransaction& tx, const CChain& activeChain, int nBlockHeight = 0 , int64_t nBlockTime = 0);


/** (client) version numbers for particular wallet features */
enum WalletFeature {
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getinfo's clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys
    FEATURE_HD = 120200,

    FEATURE_LATEST = 61000
};

enum AvailableCoinsType {
    ALL_SPENDABLE_COINS = 0,                    // find masternode outputs including locked ones (use with caution)
    STAKABLE_COINS = 1,                          // UTXO's that are valid for staking
    OWNED_VAULT_COINS = 2
};

/** Address book data */
class CAddressBookData
{
public:
    std::string name;
    std::string purpose;

    CAddressBookData()
    {
        purpose = "unknown";
    }
};

/**
 * A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */
enum TransactionCreditFilters
{
    REQUIRE_NOTHING = 0,
    REQUIRE_UNSPENT = 1,
    REQUIRE_LOCKED = 1 << 1,
    REQUIRE_UNLOCKED  = 1 << 2,
    REQUIRE_AVAILABLE_TYPE  = 1 << 3,
};
using LockedCoinsSet = std::set<COutPoint>;
using CoinVector = std::vector<COutPoint>;
using AddressBook = std::map<CTxDestination, CAddressBookData>;
using Inputs = std::vector<CTxIn>;
using Outputs = std::vector<CTxOut>;

enum TransactionNotificationType
{
    NEW = 1 << 0,
    UPDATED = 1 << 1,
    SPEND_FROM = 1 << 3,
};
class I_WalletGuiNotifications
{
public:
    virtual ~I_WalletGuiNotifications(){}
    boost::signals2::signal<void(const CTxDestination& address, const std::string& label, bool isMine, const std::string& purpose, ChangeType status)> NotifyAddressBookChanged;
    boost::signals2::signal<void(const uint256& hashTx, int status)> NotifyTransactionChanged;
    boost::signals2::signal<void(const std::string& title, int nProgress)> ShowProgress;
    boost::signals2::signal<void(bool fHaveWatchOnly)> NotifyWatchonlyChanged;
    boost::signals2::signal<void(bool fHaveMultiSig)> NotifyMultiSigChanged;
};

class AddressBookManager
{
private:
    AddressBook mapAddressBook;
public:
    const AddressBook& GetAddressBook() const;
    CAddressBookData& ModifyAddressBookData(const CTxDestination& address);
    virtual bool SetAddressBook(
        const CTxDestination& address,
        const std::string& strName,
        const std::string& purpose);

    std::set<CTxDestination> GetAccountAddresses(std::string strAccount) const;
};

class CWallet :
    public CCryptoKeyStore,
    public NotificationInterface,
    public AddressBookManager,
    public virtual I_KeypoolReserver,
    public I_WalletGuiNotifications,
    public I_StakingWallet
{
public:
    /*
     * Main wallet lock.
     * This lock protects all the fields added by CWallet
     *   except for:
     *      fFileBacked (immutable after instantiation)
     *      strWalletFile (immutable after instantiation)
     */
    mutable CCriticalSection cs_wallet;
    bool isBackedByFile() const;
    const std::string dbFilename() const;
private:
    void SetNull();

    bool fFileBacked;
    std::string strWalletFile;
    const CChain& activeChain_;
    const BlockMap& blockIndexByHash_;
    std::unique_ptr<I_MerkleTxConfirmationNumberCalculator> confirmationNumberCalculator_;
    std::unique_ptr<I_VaultManagerDatabase> vaultDB_;
    std::unique_ptr<VaultManager> vaultManager_;
    std::unique_ptr<WalletTransactionRecord> transactionRecord_;
    std::unique_ptr<SpentOutputTracker> outputTracker_;
    std::unique_ptr<I_SignatureSizeEstimator> signatureSizeEstimator_;
    std::unique_ptr<I_CoinSelectionAlgorithm> defaultCoinSelectionAlgorithm_;
    std::unique_ptr<CWalletDB> pwalletdbEncryption;

    int64_t orderedTransactionIndex;
    int nWalletVersion;   //! the current wallet version: clients below this version are not able to load the wallet
    int nWalletMaxVersion;//! the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata;

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID;

    LockedCoinsSet setLockedCoins;
    std::map<CKeyID, CHDPubKey> mapHdPubKeys; //<! memory map of HD extended pubkeys
    CPubKey vchDefaultKey;
    int64_t nTimeFirstKey;

    int64_t timeOfLastChainTipUpdate;
    int64_t nNextResend;
    int64_t nLastResend;
    std::set<int64_t> setInternalKeyPool;
    std::set<int64_t> setExternalKeyPool;
    bool walletStakingOnly;
    bool allowSpendingZeroConfirmationOutputs;
    int64_t defaultKeyPoolTopUp;

    bool SubmitTransactionToMemoryPool(const CWalletTx& wtx) const;

    void DeriveNewChildKey(const CKeyMetadata& metadata, CKey& secretRet, uint32_t nAccountIndex, bool fInternal /*= false*/);

    // Notification interface methods
    void SyncTransaction(const CTransaction& tx, const CBlock* pblock,const TransactionSyncType syncType) override;
    void RebroadcastWalletTransactions() override;
    void SetBestChain(const CBlockLocator& loc) override;
    void UpdatedBlockTip(const CBlockIndex *pindex) override;

    isminetype IsMine(const CScript& scriptPubKey) const;
    isminetype IsMine(const CTxIn& txin) const;
    bool IsMine(const CTransaction& tx) const;

public:
    void activateVaultMode();

    CKeyMetadata getKeyMetadata(const CBitcoinAddress& address) const;
    bool LoadMasterKey(unsigned int masterKeyIndex, CMasterKey& masterKey);
    bool VerifyHDKeys() const;
    bool SetAddressBook(const CTxDestination& address, const std::string& strName, const std::string& purpose) override;

    bool SetDefaultKey(const CPubKey& vchPubKey, bool updateDatabase = true);
    const CPubKey& GetDefaultKey() const;
    bool InitializeDefaultKey();

    void SetDefaultKeyTopUp(int64_t keypoolTopUp);
    void toggleSpendingZeroConfirmationOutputs();
    int64_t GetNextTransactionIndexAvailable() const;
    void UpdateNextTransactionIndexAvailable(int64_t transactionIndex);

    void UpdateTransactionMetadata(const std::vector<CWalletTx>& oldTransactions);
    void IncrementDBUpdateCount() const;

    const I_MerkleTxConfirmationNumberCalculator& getConfirmationCalculator() const;
    void UpdateBestBlockLocation();

    bool HasAgedCoins() override;
    bool SelectStakeCoins(std::set<StakableCoin>& setCoins) const override;
    bool CanStakeCoins() const override;

    bool IsSpent(const CWalletTx& wtx, unsigned int n) const;

    bool IsUnlockedForStakingOnly() const;
    bool IsFullyUnlocked() const;
    void LockFully();

    void LoadKeyPool(int nIndex, const CKeyPool &keypool);



    explicit CWallet(const CChain& chain, const BlockMap& blockMap);
    explicit CWallet(const std::string& strWalletFileIn, const CChain& chain, const BlockMap& blockMap);
    ~CWallet();


    const CWalletTx* GetWalletTx(const uint256& hash) const;
    std::vector<const CWalletTx*> GetWalletTransactionReferences() const;
    void RelayWalletTransaction(const CWalletTx& walletTransaction);

    //! check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf);
    bool IsAvailableForSpending(
        const CWalletTx* pcoin,
        unsigned int i,
        bool fIncludeZeroValue,
        bool& fIsSpendable,
        AvailableCoinsType coinType = AvailableCoinsType::ALL_SPENDABLE_COINS) const;
    bool SatisfiesMinimumDepthRequirements(const CWalletTx* pcoin, int& nDepth, bool fOnlyConfirmed) const;
    void AvailableCoins(
        std::vector<COutput>& vCoins,
        bool fOnlyConfirmed = true,
        bool fIncludeZeroValue = false,
        AvailableCoinsType nCoinType = ALL_SPENDABLE_COINS,
        CAmount nExactValue = CAmount(0)) const;
    std::map<CBitcoinAddress, std::vector<COutput> > AvailableCoinsByAddress(bool fConfirmed = true, CAmount maxCoinValue = 0);
    static bool SelectCoinsMinConf(
        const CWallet& wallet,
        const CAmount& nTargetValue,
        int nConfMine,
        int nConfTheirs,
        std::vector<COutput> vCoins,
        std::set<COutput>& setCoinsRet,
        CAmount& nValueRet);

    bool IsTrusted(const CWalletTx& walletTransaction) const;
    bool IsLockedCoin(const uint256& hash, unsigned int n) const;
    void LockCoin(const COutPoint& output);
    void UnlockCoin(const COutPoint& output);
    void UnlockAllCoins();
    void ListLockedCoins(CoinVector& vOutpts);

    //  keystore implementation
    // Generate a new key
    CPubKey GenerateNewKey(uint32_t nAccountIndex, bool fInternal);
    bool HaveKey(const CKeyID &address) const override;
    //! GetPubKey implementation that also checks the mapHdPubKeys
    bool GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const override;
    //! GetKey implementation that can derive a HD private key on the fly
    bool GetKey(const CKeyID &address, CKey& keyOut) const override;
    //! Adds a HDPubKey into the wallet(database)
    bool AddHDPubKey(const CExtPubKey &extPubKey, bool fInternal);
    //! loads a HDPubKey into the wallets memory
    bool LoadHDPubKey(const CHDPubKey &hdPubKey);
    //! Adds a key to the store, and saves it to disk.
    bool AddKeyPubKey(const CKey& key, const CPubKey& pubkey);
    //! Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key, const CPubKey& pubkey) { return CCryptoKeyStore::AddKeyPubKey(key, pubkey); }
    //! Load metadata (used by LoadWallet)
    bool LoadKeyMetadata(const CPubKey& pubkey, const CKeyMetadata& metadata, const bool updateFirstKeyTimestamp = true);

    bool LoadMinVersion(int nVersion);
    void UpdateTimeFirstKey(int64_t nCreateTime);

    //! Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret);
    //! Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret);
    bool AddCScript(const CScript& redeemScript);
    bool LoadCScript(const CScript& redeemScript);
    bool AddVault(const CScript& vaultScript, const CBlock* pblock,const CTransaction& tx);
    bool RemoveVault(const CScript& vaultScript);

    //! Adds a watch-only address to the store, and saves it to disk.
    bool AddWatchOnly(const CScript& dest);
    bool RemoveWatchOnly(const CScript& dest);
    //! Adds a watch-only address to the store, without saving it to disk (used by LoadWallet)
    bool LoadWatchOnly(const CScript& dest);

    //! Adds a MultiSig address to the store, and saves it to disk.
    bool AddMultiSig(const CScript& dest);
    bool RemoveMultiSig(const CScript& dest);
    //! Adds a MultiSig address to the store, without saving it to disk (used by LoadWallet)
    bool LoadMultiSig(const CScript& dest);

    bool Unlock(const SecureString& strWalletPassphrase, bool stakingOnly = false);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);

    void GetKeyBirthTimes(std::map<CKeyID, int64_t>& mapKeyBirth) const;

    /**
     * Increment the next transaction order id
     * @return next transaction order id
     */
    int64_t IncOrderPosNext(CWalletDB* pwalletdb = NULL);

    /**
     * Get the wallet's activity log
     * @return multimap of ordered transactions and accounting entries
     * @warning Returned pointers are *only* valid within the scope of passed acentries
     */
    typedef std::multimap<int64_t, const CWalletTx*> TxItems;
    TxItems OrderedTxItems();

    int64_t SmartWalletTxTimestampEstimation(const CWalletTx& wtxIn);
    void LoadWalletTransaction(const CWalletTx& wtxIn);
    bool AddToWallet(const CWalletTx& wtxIn,bool blockDisconnection = false);

    bool AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate,bool blockDisconnection);
    int ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate = false);
    void ReacceptWalletTransactions();
    CAmount GetBalance() const;
    CAmount GetBalanceByCoinType(AvailableCoinsType coinType) const;
    CAmount GetSpendableBalance() const;
    CAmount GetStakingBalance() const;

    CAmount GetChange(const CWalletTx& walletTransaction) const;
    CAmount GetAvailableCredit(const CWalletTx& walletTransaction, bool fUseCache = true) const;
    CAmount GetImmatureCredit(const CWalletTx& walletTransaction, bool fUseCache = true) const;
    CAmount GetUnconfirmedBalance() const;
    CAmount GetImmatureBalance() const;
    std::pair<std::string,bool> CreateTransaction(
        const std::vector<std::pair<CScript, CAmount> >& vecSend,
        CWalletTx& wtxNew,
        CReserveKey& reservekey,
        AvailableCoinsType coin_type = ALL_SPENDABLE_COINS,
        const I_CoinSelectionAlgorithm* coinSelector = nullptr);
    bool CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey);
    std::pair<std::string,bool> SendMoney(
        const std::vector<std::pair<CScript, CAmount> >& vecSend,
        CWalletTx& wtxNew,
        AvailableCoinsType coin_type = ALL_SPENDABLE_COINS,
        const I_CoinSelectionAlgorithm* coinSelector = nullptr);
    std::string PrepareObfuscationDenominate(int minRounds, int maxRounds);

    bool NewKeyPool();
    bool TopUpKeyPool(unsigned int kpSize = 0);
    bool GetKeyFromPool(CPubKey& key, bool fInternal);
    int64_t GetOldestKeyPoolTime();
    void GetAllReserveKeys(std::set<CKeyID>& setAddress) const;
    void ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool, bool fInternal) override;
    void KeepKey(int64_t nIndex) override;
    void ReturnKey(int64_t nIndex, bool fInternal) override;
    /**
     * HD Wallet Functions
     */

    /* Returns true if HD is enabled */
    bool IsHDEnabled();
    /* Generates a new HD chain */
    void GenerateNewHDChain();
    /* Set the HD chain model (chain child index counters) */
    bool SetHDChain(const CHDChain& chain, bool memonly);
    bool SetCryptedHDChain(const CHDChain& chain, bool memonly);
    bool GetDecryptedHDChain(CHDChain& hdChainRet);

    std::set<std::set<CTxDestination> > GetAddressGroupings();
    std::map<CTxDestination, CAmount> GetAddressBalances();

    bool AllInputsAreMine(const CWalletTx& walletTransaction) const;
    isminetype IsMine(const CTxOut& txout) const;
    isminetype IsMine(const CTxDestination& dest) const;

    CAmount GetDebit(const CTxIn& txin, const UtxoOwnershipFilter& filter) const;
    CAmount ComputeCredit(const CTxOut& txout, const UtxoOwnershipFilter& filter) const;
    bool IsChange(const CTxOut& txout) const;
    CAmount ComputeChange(const CTxOut& txout) const;
    bool DebitsFunds(const CTransaction& tx) const;
    bool DebitsFunds(const CWalletTx& tx,const UtxoOwnershipFilter& filter) const;

    CAmount ComputeDebit(const CTransaction& tx, const UtxoOwnershipFilter& filter) const;
    CAmount GetDebit(const CWalletTx& tx, const UtxoOwnershipFilter& filter) const;
    CAmount ComputeCredit(const CWalletTx& tx, const UtxoOwnershipFilter& filter, int creditFilterFlags = REQUIRE_NOTHING) const;
    CAmount GetCredit(const CWalletTx& walletTransaction, const UtxoOwnershipFilter& filter) const;
    CAmount ComputeChange(const CTransaction& tx) const;

    DBErrors LoadWallet(bool& fFirstRunRet);

    unsigned int GetKeyPoolSize() const;

    //! signify that a particular wallet feature is now used. this may change nWalletVersion and nWalletMaxVersion if those are lower
    bool SetMinVersion(enum WalletFeature, CWalletDB* pwalletdbIn = NULL, bool fExplicit = false);

    //! change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);

    //! get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion();

    //! Get wallet transactions that conflict with given transaction (spend same outputs)
    std::set<uint256> GetConflicts(const uint256& txid) const;
};

/**
 * Account information.
 * Stored in wallet with key "acc"+string account name.
 */
class CAccount
{
public:
    CPubKey vchPubKey;

    CAccount()
    {
        SetNull();
    }

    void SetNull()
    {
        vchPubKey = CPubKey();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPubKey);
    }
};


/**
 * Internal transfers.
 * Database key is acentry<account><counter>.
 */
class CAccountingEntry
{
private:
    void ReadOrderPos(int64_t& orderPosition, std::map<std::string,std::string>& mapping);
    void WriteOrderPos(const int64_t& orderPosition, std::map<std::string,std::string>& mapping);
public:
    std::string strAccount;
    CAmount nCreditDebit;
    int64_t nTime;
    std::string strOtherAccount;
    std::string strComment;
    std::map<std::string,std::string> mapValue;
    int64_t nOrderPos; //! position in ordered transaction list
    uint64_t nEntryNo;

    CAccountingEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
        nOrderPos = -1;
        nEntryNo = 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        //! Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit);
        READWRITE(nTime);
        READWRITE(LIMITED_STRING(strOtherAccount, 65536));

        if (!ser_action.ForRead()) {
            WriteOrderPos(nOrderPos, mapValue);

            if (!(mapValue.empty() && _ssExtra.empty())) {
                CDataStream ss(nType, nVersion);
                ss.insert(ss.begin(), '\0');
                ss << mapValue;
                ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
                strComment.append(ss.str());
            }
        }

        READWRITE(LIMITED_STRING(strComment, 65536));

        size_t nSepPos = strComment.find("\0", 0, 1);
        if (ser_action.ForRead()) {
            mapValue.clear();
            if (std::string::npos != nSepPos) {
                CDataStream ss(std::vector<char>(strComment.begin() + nSepPos + 1, strComment.end()), nType, nVersion);
                ss >> mapValue;
                _ssExtra = std::vector<char>(ss.begin(), ss.end());
            }
            ReadOrderPos(nOrderPos, mapValue);
        }
        if (std::string::npos != nSepPos)
            strComment.erase(nSepPos);

        mapValue.erase("n");
    }

private:
    std::vector<char> _ssExtra;
};

#endif // BITCOIN_WALLET_H
