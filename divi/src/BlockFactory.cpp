#include <BlockFactory.h>
#include <script/script.h>
#include <wallet.h>
#include <BlockTemplate.h>
#include <BlockMemoryPoolTransactionCollector.h>
#include <CoinstakeCreator.h>
#include <timedata.h>

// Actual mining functions
BlockFactory::BlockFactory(
    CWallet& wallet,
    int64_t& lastCoinstakeSearchInterval
    ): wallet_(wallet)
    , lastCoinstakeSearchInterval_(lastCoinstakeSearchInterval)
{

}


void BlockFactory::SetRequiredWork(CBlock& block)
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    block.nBits = GetNextWorkRequired(pindexPrev, &block,Params());
}

void BlockFactory::SetBlockTime(CBlock& block)
{
    block.nTime = GetAdjustedTime();
}

void BlockFactory::SetCoinbaseTransactionAndDefaultFees(
    std::unique_ptr<CBlockTemplate>& pblocktemplate, 
    const CMutableTransaction& coinbaseTransaction)
{
    pblocktemplate->block.vtx.push_back(coinbaseTransaction);
    pblocktemplate->vTxFees.push_back(-1);   // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end
}

void BlockFactory::CreateCoinbaseTransaction(const CScript& scriptPubKeyIn, CMutableTransaction& txNew)
{
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey = scriptPubKeyIn;
}

bool BlockFactory::AppendProofOfStakeToBlock(
    CBlock& block)
{
    CMutableTransaction txCoinStake;
    SetRequiredWork(block);
    SetBlockTime(block);

    // ppcoin: if coinstake available add coinstake tx
    static int64_t nLastCoinStakeSearchTime = GetAdjustedTime(); // only initialized at startup

    unsigned int nTxNewTime = 0;
    if(CoinstakeCreator(wallet_, lastCoinstakeSearchInterval_)
        .CreateProofOfStake(
            block.nBits, 
            block.nTime,
            nLastCoinStakeSearchTime,
            txCoinStake,
            nTxNewTime))
    {
        block.nTime = nTxNewTime;
        block.vtx[0].vout[0].SetEmpty();
        block.vtx.push_back(CTransaction(txCoinStake));
        return true;
    }

    return false;
}

CBlockTemplate* BlockFactory::CreateNewBlock(const CScript& scriptPubKeyIn, bool fProofOfStake)
{
    // Create new block
    std::unique_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if (!pblocktemplate.get())
        return NULL;
    CBlock& block = pblocktemplate->block; // pointer for convenience

    // Create coinbase tx
    CMutableTransaction coinbaseTransaction;
    CreateCoinbaseTransaction(scriptPubKeyIn, coinbaseTransaction);
    
    SetCoinbaseTransactionAndDefaultFees(pblocktemplate, coinbaseTransaction);

    if (fProofOfStake) {
        boost::this_thread::interruption_point();

        if (!AppendProofOfStakeToBlock(block))
            return NULL;
    }

    // Collect memory pool transactions into the block

    if(!BlockMemoryPoolTransactionCollector (mempool,cs_main)
        .CollectTransactionsIntoBlock(
            pblocktemplate,
            fProofOfStake,
            coinbaseTransaction
        ))
    {
        return NULL;
    }

    LogPrintf("CreateNewBlock(): releasing template %s\n", "");
    return pblocktemplate.release();
}

CBlockTemplate* BlockFactory::CreateNewBlockWithKey(CReserveKey& reservekey, bool fProofOfStake)
{
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey, false))
        return NULL;

    CScript scriptPubKey = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    return CreateNewBlock(scriptPubKey, fProofOfStake);
}