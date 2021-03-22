#define DEBUG_TYPE "objdiv"
#include <sstream>
#include <string>
#include <strstream>

#include "llvm/ADT/Statistic.h"
#include "llvm/CryptoUtils.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Obfuscation/StringObfuscation.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

STATISTIC(GlobalsEncoded, "Counts number of global variables encoded");

namespace llvm {
struct encVar {
public:
  GlobalVariable *var;
  uint8_t key;
};

class StringObfuscationPass : public llvm::ModulePass {
public:
  static char ID; // pass identification
  bool is_flag = false;
  StringObfuscationPass() : ModulePass(ID) {}
  StringObfuscationPass(bool flag) : ModulePass(ID) { is_flag = flag; }

  virtual bool runOnModule(Module &M) {
    if (!is_flag)
      return false;

    std::vector<GlobalVariable *> toDelConstGlob;
    std::vector<encVar *> encGlob;

    for (Module::global_iterator gi = M.global_begin(), ge = M.global_end();
         gi != ge; ++gi) {
      // Loop over all global variables
      GlobalVariable *gv = &(*gi);
      // errs() << "Global var " << gv->getName() << "\n";

      // std::string::size_type str_idx = gv->getName().str().find(".str.");
      std::string section(gv->getSection());

      const static auto str_marker = "??_C@"; // SHOULD START WITH: ??_C@

      // Let's encode the static ones
      const auto gv_name = gv->getName().str();

      if (gv_name.substr(0, 5) == str_marker && gv->isConstant() && gv->hasInitializer() && isa<ConstantDataSequential>(gv->getInitializer()) && section != "llvm.metadata" && section.find("__objc_methname") == std::string::npos ) 
      {
        const auto is_unicode = gv->getAlignment() > 1;

        if( is_unicode )
          continue;

        ++GlobalsEncoded;
        //  errs() << " is constant" << "\n";

        // Duplicate global variable
        GlobalVariable *dynGV = new GlobalVariable(
            M, gv->getType()->getElementType(), !(gv->isConstant()),
            gv->getLinkage(), (Constant *)0, gv->getName(), (GlobalVariable *)0,
            gv->getThreadLocalMode(), gv->getType()->getAddressSpace());
        // dynGV->copyAttributesFrom(gv);
        dynGV->setInitializer(gv->getInitializer());

       // errs() << "GV: " << *gv << "\n";

        Constant *initializer = gv->getInitializer();
        ConstantDataSequential *cdata =
            dyn_cast<ConstantDataSequential>(initializer);
        if (cdata) 
        {
          const auto orig = cdata->getRawDataValues().data();
          size_t len = cdata->getNumElements() * cdata->getElementByteSize();

          encVar *cur = new encVar();
          cur->var = dynGV;
          cur->key = llvm::cryptoutils->get_uint8_t();
          cur->is_unicode = is_unicode;
          
          // casting away const is undef. behavior in C++
          // TODO a clean implementation would retrieve the data, generate a new
          // constant set the correct type, and copy the data over.
          // char *encr = new char[len];
          // Constant *initnew = ConstantDataArray::getString(M.getContext(),
          // encr, true);

          // Simple xor encoding
          char* encr = const_cast< char*>( orig );

          for (size_t i = 0; i != len; ++i) 
              encr[i] ^= cur->key;        

          // FIXME Second part of the unclean hack.
          dynGV->setInitializer(initializer);

          // Prepare to add decode function for this variable
          encGlob.push_back(cur);
        } else {
          // just copying default initializer for now
          dynGV->setInitializer(initializer);
        }

        // redirect references to new GV and remove old one
        gv->replaceAllUsesWith(dynGV);
        toDelConstGlob.push_back(gv);
      }
    }

    // actuallte delete marked globals
    for (unsigned i = 0, e = toDelConstGlob.size(); i != e; ++i)
      toDelConstGlob[i]->eraseFromParent();

    //  errs() << "Enc glob count " << encGlob.size() << "\n";

    addDecodeFunction(&M, &encGlob);

    return true;
  }

private:
  void addDecodeFunction(Module *mod, std::vector<encVar *> *gvars) {
    // Declare and add the function definition
    // errs()<<"Successful enter decode function"<<"\n";
    std::vector<Type *> FuncTy_args;
    FunctionType *FuncTy = FunctionType::get(
        /*Result=*/Type::getVoidTy(mod->getContext()), // returning void
        /*Params=*/FuncTy_args,                        // taking no args
        /*isVarArg=*/false);
    uint64_t StringObfDecodeRandomName = cryptoutils->get_uint64_t();
    std::string random_str;
    std::stringstream random_stream;
    random_stream << StringObfDecodeRandomName;
    random_stream >> random_str;
    StringObfDecodeRandomName++;

    auto C = mod->getOrInsertFunction(".strobf_decode" + random_str, FuncTy);
    Function *fdecode = cast<Function>(C.getCallee());
    fdecode->setCallingConv(CallingConv::C);

    BasicBlock *entry = BasicBlock::Create(mod->getContext(), "entry", fdecode);

    IRBuilder<> builder(mod->getContext());
    builder.SetInsertPoint(entry);

    for (unsigned i = 0, e = gvars->size(); i != e; ++i) {
      GlobalVariable *gvar = (*gvars)[i]->var;
      uint8_t key = (*gvars)[i]->key;

      Constant *init = gvar->getInitializer();
      ConstantDataSequential *cdata = dyn_cast<ConstantDataSequential>(init);

      unsigned len = cdata->getNumElements() * cdata->getElementByteSize();
      --len;

      BasicBlock *preHeaderBB = builder.GetInsertBlock();
      BasicBlock *for_body =
          BasicBlock::Create(mod->getContext(), "for-body", fdecode);
      BasicBlock *for_end =
          BasicBlock::Create(mod->getContext(), "for-end", fdecode);
      builder.CreateBr(for_body);
      builder.SetInsertPoint(for_body);
      PHINode *variable =
          builder.CreatePHI(Type::getInt32Ty(mod->getContext()), 2, "i");
      Value *startValue = builder.getInt32(0);
      Value *endValue = builder.getInt32(len);
      variable->addIncoming(startValue, preHeaderBB);
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

      // LoadInst *Load=builder.CreateLoad(gvar);
      // errs()<<"Load: "<<*(Load->getPointerOperand())<<"\n";
      Value *indexList[2] = {ConstantInt::get(variable->getType(), 0),
                             variable};

      Value *const_key = builder.getInt8(key);

      Value *GEP =
          builder.CreateGEP(gvar, ArrayRef<Value *>(indexList, 2), "arrayIdx");
      LoadInst *loadElement = builder.CreateLoad(GEP, false);

      loadElement->setAlignment(Align(1));

      Value *Xor = builder.CreateXor(loadElement, const_key, "xor");
      StoreInst *Store = builder.CreateStore(Xor, GEP, false);

      Store->setAlignment(Align(1));

      ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      Value *stepValue = builder.getInt32(1);
      Value *nextValue = builder.CreateAdd(variable, stepValue, "next-value");
      Value *endCondition =
          builder.CreateICmpULT(variable, endValue, "end-condition");
      endCondition = builder.CreateICmpNE(endCondition, builder.getInt1(0),
                                          "loop-condition");
      BasicBlock *loopEndBB = builder.GetInsertBlock();
      builder.CreateCondBr(endCondition, loopEndBB, for_end);
      builder.SetInsertPoint(for_end);
      variable->addIncoming(nextValue, loopEndBB);
    }
    builder.CreateRetVoid();
    appendToGlobalCtors(*mod, fdecode, 0);
  }
};
} // namespace llvm

char StringObfuscationPass::ID = 0;
static RegisterPass<StringObfuscationPass>
    X("GVDiv", "Global variable (i.e., const char*) diversification pass",
      false, true);

Pass *llvm::createStringObfuscationPass(bool flag) {
  return new StringObfuscationPass(flag);
}