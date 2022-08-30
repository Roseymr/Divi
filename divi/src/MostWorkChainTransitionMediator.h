#ifndef MOST_WORK_CHAIN_TRANSITION_MEDIATOR_H
#define MOST_WORK_CHAIN_TRANSITION_MEDIATOR_H
#include <BlockCheckingHelpers.h>

class CValidationState;
class CBlock;
class CBlockIndex;
class ChainstateManager;
class I_ChainTipManager;

class I_MostWorkChainTipLocator
{
public:
    virtual ~I_MostWorkChainTipLocator(){}
    virtual CBlockIndex* findMostWorkChain() const = 0;
};

class I_MostWorkChainTransitionMediator: public I_MostWorkChainTipLocator
{
public:
    virtual ~I_MostWorkChainTransitionMediator(){}
    virtual bool transitionActiveChainToMostWorkChain(
            CBlockIndex* pindexMostWork,
            const CBlock* pblock) const = 0;
};

class MostWorkChainTransitionMediator final: public I_MostWorkChainTransitionMediator
{
private:
    ChainstateManager& chainstate_;
    BlockIndexSuccessorsByPreviousBlockIndex& unlinkedBlocks_;
    BlockIndexCandidates& blockIndexCandidates_;
    CValidationState& state_;
    const I_ChainTipManager& chainTipManager_;

    void computeNextBlockIndicesToConnect(
        CBlockIndex* pindexMostWork,
        const int startingHeight,
        const int maxHeightTarget,
        std::vector<CBlockIndex*>& blockIndicesToConnect) const;

    bool rollBackChainTipToConnectToMostWorkChain(
        const CChain& chain,
        const CBlockIndex* mostWorkBlockIndex) const;
    bool checkBlockConnectionState(CBlockIndex* lastBlockIndex) const;

    enum class BlockConnectionResult
    {
        TRY_NEXT_BLOCK,
        UNKNOWN_SYSTEM_ERROR,
        INVALID_BLOCK,
        CHAINWORK_IMPROVED,
    };

    BlockConnectionResult tryToConnectNextBlock(
        const CChain& chain,
        const CBlock* blockToConnect,
        const CBlockIndex* previousChainTip,
        CBlockIndex* proposedNewChainTip,
        CBlockIndex* pindexConnect) const;


public:
    MostWorkChainTransitionMediator(
        ChainstateManager& chainstate,
        BlockIndexSuccessorsByPreviousBlockIndex& unlinkedBlocks,
        BlockIndexCandidates& blockIndexCandidates,
        CValidationState& state,
        const I_ChainTipManager& chainTipManager);

    bool transitionActiveChainToMostWorkChain(
        CBlockIndex* pindexMostWork,
        const CBlock* pblock) const override;
    CBlockIndex* findMostWorkChain() const override;
};
#endif// MOST_WORK_CHAIN_TRANSITION_MEDIATOR_H