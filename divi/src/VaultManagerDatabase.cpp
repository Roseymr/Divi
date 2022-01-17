#include <VaultManagerDatabase.h>

#include <utility>
#include <WalletTx.h>
#include <DataDirectory.h>
#include <sync.h>

static std::pair<std::string,uint64_t> MakeTxIndex(uint64_t txIndex)
{
    return std::make_pair("tx",txIndex);
}
static std::pair<std::string,uint64_t> MakeScriptIndex(uint64_t scriptIndex)
{
    return std::make_pair("script",scriptIndex);
}

VaultManagerDatabase::VaultManagerDatabase(
    std::string vaultID,
    size_t nCacheSize,
    bool fMemory,
    bool fWipe
    ):  CLevelDBWrapper(GetDataDir() / vaultID, nCacheSize, fMemory, fWipe)
    , txCount(0u)
    , scriptIDLookup()
{
}

bool VaultManagerDatabase::WriteTx(const CWalletTx& walletTransaction)
{
    return Write(MakeTxIndex(txCount++),walletTransaction);
}

bool VaultManagerDatabase::ReadTx(CWalletTx& walletTransaction)
{
    return Read(MakeTxIndex(txCount++),walletTransaction);
}

bool VaultManagerDatabase::WriteManagedScript(const CScript& managedScript)
{
    const CScriptID id(managedScript);
    if(scriptIDLookup.count(id) == 0u)
    {
        uint64_t nextIndex = scriptIDLookup.size();
        if(Write(MakeScriptIndex(nextIndex),managedScript))
        {
            scriptIDLookup[id] =  nextIndex;
            return true;
        }
        return false;
    }
    return false;
}

bool VaultManagerDatabase::EraseManagedScript(const CScript& managedScript)
{
    const CScriptID id(managedScript);
    if(scriptIDLookup.count(id) > 0)
    {
        if(Erase(MakeScriptIndex(scriptIDLookup[id])))
        {
            scriptIDLookup.erase(id);
            return true;
        }
        return false;
    }
    return false;
}

bool VaultManagerDatabase::ReadManagedScripts(ManagedScripts& managedScripts)
{
    uint64_t dummyScriptIndex = 0u;
    CScript managedScript;
    while(Read(MakeScriptIndex(dummyScriptIndex),managedScript))
    {
        CScriptID id(managedScript);
        scriptIDLookup[id] = dummyScriptIndex;

        managedScripts.insert(managedScript);
        managedScript.clear();
        ++dummyScriptIndex;
    }
    return true;
}

bool VaultManagerDatabase::Sync()
{
    return CLevelDBWrapper::Sync();
}