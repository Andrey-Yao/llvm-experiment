#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <llvm/IR/BasicBlock.h>
using namespace llvm;

namespace {
  struct SkeletonPass : public FunctionPass {
    static char ID;
    SkeletonPass() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F) {
      unsigned int changed = 0;
      for (auto& B : F) {
	for (auto& I : B) {
	  if (auto* instr = dyn_cast<BinaryOperator>(&I)) {
	    if(instr->getOpcode() == Instruction::Mul){
	      LLVMContext &cntxt = F.getContext();

	      Value *lhs = instr->getOperand(0);
	      Value *rhs = instr->getOperand(1);

	      Type *intype = lhs->getType();
	      
	      DomTreeUpdater *dtu = nullptr;
	      Twine aftblklbl = Twine(B.getName()) + Twine("_after");

	      //Splits the block at the mul instruction
	      BasicBlock *afterBlock =
		SplitBlock(&B, instr, dtu, 0, 0, Twine(aftblklbl));

	      Twine nlbl = Twine(B.getName()) + Twine("_diamondN");
	      Twine wlbl = Twine(B.getName()) + Twine("_diamondW");
	      Twine elbl = Twine(B.getName()) + Twine("_diamondE");
	      Twine slbl = Twine(B.getName()) + Twine("_diamondS");

	      //Four diamond blocks for ternary operation basically
	      BasicBlock *diamondN = BasicBlock::Create(cntxt, nlbl);
	      BasicBlock *diamondW = BasicBlock::Create(cntxt, wlbl);
	      BasicBlock *diamondE = BasicBlock::Create(cntxt, elbl);
	      BasicBlock *diamondS = BasicBlock::Create(cntxt, slbl);
	      
	      B.getTerminator()->setSuccessor(0, diamondN);

	      IRBuilder<> builderN(diamondN);
	      Value *zero = ConstantInt::get(intype, 0);
	      Value *cond = builderN.CreateICmpSLE(zero, lhs);
	      builderN.CreateCondBr(cond, diamondW, diamondE);

	      IRBuilder<> builderW(diamondW);
	      Value *neg_one = ConstantInt::get(intype, -1);
	      builderW.Insert(neg_one);
	      builderW.CreateBr(diamondS);

	      IRBuilder<> builderE(diamondW);
	      Value *pos_one = ConstantInt::get(intype, 1);
	      builderE.Insert(pos_one);
	      builderE.CreateBr(diamondS);

	      IRBuilder<> builderS(diamondW);
	      PHINode *offset = builderS.CreatePHI(intype, 2);
	      offset->addIncoming(neg_one, diamondW);
	      offset->addIncoming(pos_one, diamondE);
	      
	      Twine forlbl = Twine(B.getName()) + Twine("_forloopy");
	      BasicBlock* forLoop =
		BasicBlock::Create(F.getContext(), forlbl, &F);
	      builderS.CreateBr(forLoop);
	      IRBuilder<> buildFor(forLoop);
	      PHINode *phiCount = buildFor.CreatePHI(intype, 2);
	      PHINode *phiAccum = buildFor.CreatePHI(intype, 2);
	      Value* incrCount = buildFor.CreateAdd(phiCount, offset);
	      Value* incrAccum = buildFor.CreateAdd(phiAccum, rhs);
	      phiCount->addIncoming(zero, diamondS);
	      phiCount->addIncoming(incrCount, forLoop);
	      phiAccum->addIncoming(zero, diamondS);
	      phiAccum->addIncoming(incrAccum, forLoop);
	      Value* loopGuard = buildFor.CreateICmpEQ(phiCount, zero);
	      buildFor.CreateCondBr(loopGuard, afterBlock, forLoop);

	      //Replace references to the old mul with new value phiAccum
	      for (auto& U : instr->uses()) {
		User* user = U.getUser();
		user->setOperand(U.getOperandNo(), phiAccum);
	      }
	      
	      //Updates phi nodes after this block
	      afterBlock->replaceSuccessorsPhiUsesWith(&B, afterBlock);

	      //Deletes instruction
	      instr->eraseFromParent();

	      //Record that we yeeted a multiplication
	      changed ++;
	    }
	  }
	}
      }
      if(changed == 0){
	return false;
	errs() << "No mul values found";
      }else{
	errs() << "Yeeted " << changed
	       << " mul instructions and replaced them with repeated additions";
	return true;
      };
    }
  };
}

char SkeletonPass::ID = 0;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerSkeletonPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  PM.add(new SkeletonPass());
}
static RegisterStandardPasses
  RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
                 registerSkeletonPass);
