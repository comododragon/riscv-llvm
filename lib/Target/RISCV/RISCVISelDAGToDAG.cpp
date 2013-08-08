//===-- RISCVISelDAGToDAG.cpp - A dag to dag inst selector for RISCV --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the RISCV target.
//
//===----------------------------------------------------------------------===//

#include "RISCVTargetMachine.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
// Used to build addressing modes.
struct RISCVAddressingMode {
  // The shape of the address.
  enum AddrForm {
    // base+displacement
    FormBD,

    // base+displacement+index for load and store operands
    FormBDXNormal,

    // base+displacement+index for load address operands
    FormBDXLA,

    // base+displacement+index+ADJDYNALLOC
    FormBDXDynAlloc
  };
  AddrForm Form;

  // The type of displacement.  The enum names here correspond directly
  // to the definitions in RISCVOperand.td.  We could split them into
  // flags -- single/pair, 128-bit, etc. -- but it hardly seems worth it.
  enum DispRange {
    Disp12Only,
    Disp12Pair,
    Disp20Only,
    Disp20Only128,
    Disp20Pair
  };
  DispRange DR;

  // The parts of the address.  The address is equivalent to:
  //
  //     Base + Disp + Index + (IncludesDynAlloc ? ADJDYNALLOC : 0)
  SDValue Base;
  int64_t Disp;
  SDValue Index;
  bool IncludesDynAlloc;

  RISCVAddressingMode(AddrForm form, DispRange dr)
    : Form(form), DR(dr), Base(), Disp(0), Index(),
      IncludesDynAlloc(false) {}

  // True if the address can have an index register.
  bool hasIndexField() { return Form != FormBD; }

  // True if the address can (and must) include ADJDYNALLOC.
  bool isDynAlloc() { return Form == FormBDXDynAlloc; }

  void dump() {
    errs() << "RISCVAddressingMode " << this << '\n';

    errs() << " Base ";
    if (Base.getNode() != 0)
      Base.getNode()->dump();
    else
      errs() << "null\n";

    if (hasIndexField()) {
      errs() << " Index ";
      if (Index.getNode() != 0)
        Index.getNode()->dump();
      else
        errs() << "null\n";
    }

    errs() << " Disp " << Disp;
    if (IncludesDynAlloc)
      errs() << " + ADJDYNALLOC";
    errs() << '\n';
  }
};

class RISCVDAGToDAGISel : public SelectionDAGISel {
  const RISCVTargetLowering &Lowering;
  const RISCVSubtarget &Subtarget;

  // Used by RISCVOperands.td to create integer constants.
  inline SDValue getImm(const SDNode *Node, uint64_t Imm) {
    return CurDAG->getTargetConstant(Imm, Node->getValueType(0));
  }

  // Try to fold more of the base or index of AM into AM, where IsBase
  // selects between the base and index.
  bool expandAddress(RISCVAddressingMode &AM, bool IsBase);

  // Try to describe N in AM, returning true on success.
  bool selectAddress(SDValue N, RISCVAddressingMode &AM);

  // Extract individual target operands from matched address AM.
  void getAddressOperands(const RISCVAddressingMode &AM, EVT VT,
                          SDValue &Base, SDValue &Disp);
  void getAddressOperands(const RISCVAddressingMode &AM, EVT VT,
                          SDValue &Base, SDValue &Disp, SDValue &Index);

  //RISCV
  bool selectMemRegAddr(SDValue Addr, SDValue &Base, SDValue &Offset) {
      
    EVT ValTy = Addr.getValueType();

    // if Address is FI, get the TargetFrameIndex.
    if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
      Base   = CurDAG->getTargetFrameIndex(FIN->getIndex(), ValTy);
      Offset = CurDAG->getTargetConstant(0, ValTy);
      return true;
    }

    if (TM.getRelocationModel() != Reloc::PIC_) {
      if ((Addr.getOpcode() == ISD::TargetExternalSymbol ||
          Addr.getOpcode() == ISD::TargetGlobalAddress))
        return false;
    }

    // Addresses of the form FI+const or FI|const
    if (CurDAG->isBaseWithConstantOffset(Addr)) {
      ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1));
      if (isInt<16>(CN->getSExtValue())) {
  
        // If the first operand is a FI, get the TargetFI Node
        if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>
                                    (Addr.getOperand(0)))
          Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), ValTy);
        else
          Base = Addr.getOperand(0);
  
        Offset = CurDAG->getTargetConstant(CN->getZExtValue(), ValTy);
        return true;
      }
    }

    //Last case
    Base = Addr;
    Offset = CurDAG->getTargetConstant(0, Addr.getValueType());
    return true;
  }
  //End RISCV

  // Try to match Addr as a FormBD address with displacement type DR.
  // Return true on success, storing the base and displacement in
  // Base and Disp respectively.
  bool selectBDAddr(RISCVAddressingMode::DispRange DR, SDValue Addr,
                    SDValue &Base, SDValue &Disp);

  // Try to match Addr as a FormBDX* address of form Form with
  // displacement type DR.  Return true on success, storing the base,
  // displacement and index in Base, Disp and Index respectively.
  bool selectBDXAddr(RISCVAddressingMode::AddrForm Form,
                     RISCVAddressingMode::DispRange DR, SDValue Addr,
                     SDValue &Base, SDValue &Disp, SDValue &Index);

  // PC-relative address matching routines used by RISCVOperands.td.
  bool selectPCRelAddress(SDValue Addr, SDValue &Target) {
    if (Addr.getOpcode() == RISCVISD::PCREL_WRAPPER) {
      Target = Addr.getOperand(0);
      return true;
    }
    return false;
  }

  // BD matching routines used by RISCVOperands.td.
  bool selectBDAddr12Only(SDValue Addr, SDValue &Base, SDValue &Disp) {
    return selectBDAddr(RISCVAddressingMode::Disp12Only, Addr, Base, Disp);
  }
  bool selectBDAddr12Pair(SDValue Addr, SDValue &Base, SDValue &Disp) {
    return selectBDAddr(RISCVAddressingMode::Disp12Pair, Addr, Base, Disp);
  }
  bool selectBDAddr20Only(SDValue Addr, SDValue &Base, SDValue &Disp) {
    return selectBDAddr(RISCVAddressingMode::Disp20Only, Addr, Base, Disp);
  }
  bool selectBDAddr20Pair(SDValue Addr, SDValue &Base, SDValue &Disp) {
    return selectBDAddr(RISCVAddressingMode::Disp20Pair, Addr, Base, Disp);
  }

  // BDX matching routines used by RISCVOperands.td.
  bool selectBDXAddr12Only(SDValue Addr, SDValue &Base, SDValue &Disp,
                           SDValue &Index) {
    return selectBDXAddr(RISCVAddressingMode::FormBDXNormal,
                         RISCVAddressingMode::Disp12Only,
                         Addr, Base, Disp, Index);
  }
  bool selectBDXAddr12Pair(SDValue Addr, SDValue &Base, SDValue &Disp,
                           SDValue &Index) {
    return selectBDXAddr(RISCVAddressingMode::FormBDXNormal,
                         RISCVAddressingMode::Disp12Pair,
                         Addr, Base, Disp, Index);
  }
  bool selectDynAlloc12Only(SDValue Addr, SDValue &Base, SDValue &Disp,
                            SDValue &Index) {
    return selectBDXAddr(RISCVAddressingMode::FormBDXDynAlloc,
                         RISCVAddressingMode::Disp12Only,
                         Addr, Base, Disp, Index);
  }
  bool selectBDXAddr20Only(SDValue Addr, SDValue &Base, SDValue &Disp,
                           SDValue &Index) {
    return selectBDXAddr(RISCVAddressingMode::FormBDXNormal,
                         RISCVAddressingMode::Disp20Only,
                         Addr, Base, Disp, Index);
  }
  bool selectBDXAddr20Only128(SDValue Addr, SDValue &Base, SDValue &Disp,
                              SDValue &Index) {
    return selectBDXAddr(RISCVAddressingMode::FormBDXNormal,
                         RISCVAddressingMode::Disp20Only128,
                         Addr, Base, Disp, Index);
  }
  bool selectBDXAddr20Pair(SDValue Addr, SDValue &Base, SDValue &Disp,
                           SDValue &Index) {
    return selectBDXAddr(RISCVAddressingMode::FormBDXNormal,
                         RISCVAddressingMode::Disp20Pair,
                         Addr, Base, Disp, Index);
  }
  bool selectLAAddr12Pair(SDValue Addr, SDValue &Base, SDValue &Disp,
                          SDValue &Index) {
    return selectBDXAddr(RISCVAddressingMode::FormBDXLA,
                         RISCVAddressingMode::Disp12Pair,
                         Addr, Base, Disp, Index);
  }
  bool selectLAAddr20Pair(SDValue Addr, SDValue &Base, SDValue &Disp,
                          SDValue &Index) {
    return selectBDXAddr(RISCVAddressingMode::FormBDXLA,
                         RISCVAddressingMode::Disp20Pair,
                         Addr, Base, Disp, Index);
  }

  // If Op0 is null, then Node is a constant that can be loaded using:
  //
  //   (Opcode UpperVal LowerVal)
  //
  // If Op0 is nonnull, then Node can be implemented using:
  //
  //   (Opcode (Opcode Op0 UpperVal) LowerVal)
  SDNode *splitLargeImmediate(unsigned Opcode, SDNode *Node, SDValue Op0,
                              uint64_t UpperVal, uint64_t LowerVal);

public:
  RISCVDAGToDAGISel(RISCVTargetMachine &TM, CodeGenOpt::Level OptLevel)
    : SelectionDAGISel(TM, OptLevel),
      Lowering(*TM.getTargetLowering()),
      Subtarget(*TM.getSubtargetImpl()) { }

  // Override MachineFunctionPass.
  virtual const char *getPassName() const LLVM_OVERRIDE {
    return "RISCV DAG->DAG Pattern Instruction Selection";
  }

  // Override SelectionDAGISel.
  virtual SDNode *Select(SDNode *Node) LLVM_OVERRIDE;
  virtual bool SelectInlineAsmMemoryOperand(const SDValue &Op,
                                            char ConstraintCode,
                                            std::vector<SDValue> &OutOps)
    LLVM_OVERRIDE;

  // Include the pieces autogenerated from the target description.
  #include "RISCVGenDAGISel.inc"
};
} // end anonymous namespace

FunctionPass *llvm::createRISCVISelDag(RISCVTargetMachine &TM,
                                         CodeGenOpt::Level OptLevel) {
  return new RISCVDAGToDAGISel(TM, OptLevel);
}

// Return true if Val should be selected as a displacement for an address
// with range DR.  Here we're interested in the range of both the instruction
// described by DR and of any pairing instruction.
static bool selectDisp(RISCVAddressingMode::DispRange DR, int64_t Val) {
  switch (DR) {
  case RISCVAddressingMode::Disp12Only:
    return isUInt<12>(Val);

  case RISCVAddressingMode::Disp12Pair:
  case RISCVAddressingMode::Disp20Only:
  case RISCVAddressingMode::Disp20Pair:
    return isInt<20>(Val);

  case RISCVAddressingMode::Disp20Only128:
    return isInt<20>(Val) && isInt<20>(Val + 8);
  }
  llvm_unreachable("Unhandled displacement range");
}

// Change the base or index in AM to Value, where IsBase selects
// between the base and index.
static void changeComponent(RISCVAddressingMode &AM, bool IsBase,
                            SDValue Value) {
  if (IsBase)
    AM.Base = Value;
  else
    AM.Index = Value;
}

// The base or index of AM is equivalent to Value + ADJDYNALLOC,
// where IsBase selects between the base and index.  Try to fold the
// ADJDYNALLOC into AM.
static bool expandAdjDynAlloc(RISCVAddressingMode &AM, bool IsBase,
                              SDValue Value) {
  if (AM.isDynAlloc() && !AM.IncludesDynAlloc) {
    changeComponent(AM, IsBase, Value);
    AM.IncludesDynAlloc = true;
    return true;
  }
  return false;
}

// The base of AM is equivalent to Base + Index.  Try to use Index as
// the index register.
static bool expandIndex(RISCVAddressingMode &AM, SDValue Base,
                        SDValue Index) {
  if (AM.hasIndexField() && !AM.Index.getNode()) {
    AM.Base = Base;
    AM.Index = Index;
    return true;
  }
  return false;
}

// The base or index of AM is equivalent to Op0 + Op1, where IsBase selects
// between the base and index.  Try to fold Op1 into AM's displacement.
static bool expandDisp(RISCVAddressingMode &AM, bool IsBase,
                       SDValue Op0, ConstantSDNode *Op1) {
  // First try adjusting the displacement.
  int64_t TestDisp = AM.Disp + Op1->getSExtValue();
  if (selectDisp(AM.DR, TestDisp)) {
    changeComponent(AM, IsBase, Op0);
    AM.Disp = TestDisp;
    return true;
  }

  // We could consider forcing the displacement into a register and
  // using it as an index, but it would need to be carefully tuned.
  return false;
}

bool RISCVDAGToDAGISel::expandAddress(RISCVAddressingMode &AM,
                                        bool IsBase) {
  SDValue N = IsBase ? AM.Base : AM.Index;
  unsigned Opcode = N.getOpcode();
  if (Opcode == ISD::TRUNCATE) {
    N = N.getOperand(0);
    Opcode = N.getOpcode();
  }
  if (Opcode == ISD::ADD || CurDAG->isBaseWithConstantOffset(N)) {
    SDValue Op0 = N.getOperand(0);
    SDValue Op1 = N.getOperand(1);

    unsigned Op0Code = Op0->getOpcode();
    unsigned Op1Code = Op1->getOpcode();

    if (Op0Code == RISCVISD::ADJDYNALLOC)
      return expandAdjDynAlloc(AM, IsBase, Op1);
    if (Op1Code == RISCVISD::ADJDYNALLOC)
      return expandAdjDynAlloc(AM, IsBase, Op0);

    if (Op0Code == ISD::Constant)
      return expandDisp(AM, IsBase, Op1, cast<ConstantSDNode>(Op0));
    if (Op1Code == ISD::Constant)
      return expandDisp(AM, IsBase, Op0, cast<ConstantSDNode>(Op1));

    if (IsBase && expandIndex(AM, Op0, Op1))
      return true;
  }
  return false;
}

// Return true if an instruction with displacement range DR should be
// used for displacement value Val.  selectDisp(DR, Val) must already hold.
static bool isValidDisp(RISCVAddressingMode::DispRange DR, int64_t Val) {
  assert(selectDisp(DR, Val) && "Invalid displacement");
  switch (DR) {
  case RISCVAddressingMode::Disp12Only:
  case RISCVAddressingMode::Disp20Only:
  case RISCVAddressingMode::Disp20Only128:
    return true;

  case RISCVAddressingMode::Disp12Pair:
    // Use the other instruction if the displacement is too large.
    return isUInt<12>(Val);

  case RISCVAddressingMode::Disp20Pair:
    // Use the other instruction if the displacement is small enough.
    return !isUInt<12>(Val);
  }
  llvm_unreachable("Unhandled displacement range");
}

// Return true if Base + Disp + Index should be performed by LA(Y).
static bool shouldUseLA(SDNode *Base, int64_t Disp, SDNode *Index) {
  // Don't use LA(Y) for constants.
  if (!Base)
    return false;

  // Always use LA(Y) for frame addresses, since we know that the destination
  // register is almost always (perhaps always) going to be different from
  // the frame register.
  if (Base->getOpcode() == ISD::FrameIndex)
    return true;

  if (Disp) {
    // Always use LA(Y) if there is a base, displacement and index.
    if (Index)
      return true;

    // Always use LA if the displacement is small enough.  It should always
    // be no worse than AGHI (and better if it avoids a move).
    if (isUInt<12>(Disp))
      return true;

    // For similar reasons, always use LAY if the constant is too big for AGHI.
    // LAY should be no worse than AGFI.
    if (!isInt<16>(Disp))
      return true;
  } else {
    // Don't use LA for plain registers.
    if (!Index)
      return false;

    // Don't use LA for plain addition if the index operand is only used
    // once.  It should be a natural two-operand addition in that case.
    if (Index->hasOneUse())
      return false;

    // Prefer addition if the second operation is sign-extended, in the
    // hope of using AGF.
    unsigned IndexOpcode = Index->getOpcode();
    if (IndexOpcode == ISD::SIGN_EXTEND ||
        IndexOpcode == ISD::SIGN_EXTEND_INREG)
      return false;
  }

  // Don't use LA for two-operand addition if either operand is only
  // used once.  The addition instructions are better in that case.
  if (Base->hasOneUse())
    return false;

  return true;
}

// Return true if Addr is suitable for AM, updating AM if so.
bool RISCVDAGToDAGISel::selectAddress(SDValue Addr,
                                        RISCVAddressingMode &AM) {
  // Start out assuming that the address will need to be loaded separately,
  // then try to extend it as much as we can.
  AM.Base = Addr;

  // First try treating the address as a constant.
  if (Addr.getOpcode() == ISD::Constant &&
      expandDisp(AM, true, SDValue(), cast<ConstantSDNode>(Addr)))
    ;
  else
    // Otherwise try expanding each component.
    while (expandAddress(AM, true) ||
           (AM.Index.getNode() && expandAddress(AM, false)))
      continue;

  // Reject cases where it isn't profitable to use LA(Y).
  if (AM.Form == RISCVAddressingMode::FormBDXLA &&
      !shouldUseLA(AM.Base.getNode(), AM.Disp, AM.Index.getNode()))
    return false;

  // Reject cases where the other instruction in a pair should be used.
  if (!isValidDisp(AM.DR, AM.Disp))
    return false;

  // Make sure that ADJDYNALLOC is included where necessary.
  if (AM.isDynAlloc() && !AM.IncludesDynAlloc)
    return false;

  DEBUG(AM.dump());
  return true;
}

// Insert a node into the DAG at least before Pos.  This will reposition
// the node as needed, and will assign it a node ID that is <= Pos's ID.
// Note that this does *not* preserve the uniqueness of node IDs!
// The selection DAG must no longer depend on their uniqueness when this
// function is used.
static void insertDAGNode(SelectionDAG *DAG, SDNode *Pos, SDValue N) {
  if (N.getNode()->getNodeId() == -1 ||
      N.getNode()->getNodeId() > Pos->getNodeId()) {
    DAG->RepositionNode(Pos, N.getNode());
    N.getNode()->setNodeId(Pos->getNodeId());
  }
}

void RISCVDAGToDAGISel::getAddressOperands(const RISCVAddressingMode &AM,
                                             EVT VT, SDValue &Base,
                                             SDValue &Disp) {
  Base = AM.Base;
  if (!Base.getNode())
    // Register 0 means "no base".  This is mostly useful for shifts.
    Base = CurDAG->getRegister(0, VT);
  else if (Base.getOpcode() == ISD::FrameIndex) {
    // Lower a FrameIndex to a TargetFrameIndex.
    int64_t FrameIndex = cast<FrameIndexSDNode>(Base)->getIndex();
    Base = CurDAG->getTargetFrameIndex(FrameIndex, VT);
  } else if (Base.getValueType() != VT) {
    // Truncate values from i64 to i32, for shifts.
    assert(VT == MVT::i32 && Base.getValueType() == MVT::i64 &&
           "Unexpected truncation");
    DebugLoc DL = Base.getDebugLoc();
    SDValue Trunc = CurDAG->getNode(ISD::TRUNCATE, DL, VT, Base);
    insertDAGNode(CurDAG, Base.getNode(), Trunc);
    Base = Trunc;
  }

  // Lower the displacement to a TargetConstant.
  Disp = CurDAG->getTargetConstant(AM.Disp, VT);
}

void RISCVDAGToDAGISel::getAddressOperands(const RISCVAddressingMode &AM,
                                             EVT VT, SDValue &Base,
                                             SDValue &Disp, SDValue &Index) {
  getAddressOperands(AM, VT, Base, Disp);

  Index = AM.Index;
  if (!Index.getNode())
    // Register 0 means "no index".
    Index = CurDAG->getRegister(0, VT);
}

bool RISCVDAGToDAGISel::selectBDAddr(RISCVAddressingMode::DispRange DR,
                                       SDValue Addr, SDValue &Base,
                                       SDValue &Disp) {
  RISCVAddressingMode AM(RISCVAddressingMode::FormBD, DR);
  if (!selectAddress(Addr, AM))
    return false;

  getAddressOperands(AM, Addr.getValueType(), Base, Disp);
  return true;
}

bool RISCVDAGToDAGISel::selectBDXAddr(RISCVAddressingMode::AddrForm Form,
                                        RISCVAddressingMode::DispRange DR,
                                        SDValue Addr, SDValue &Base,
                                        SDValue &Disp, SDValue &Index) {
  RISCVAddressingMode AM(Form, DR);
  if (!selectAddress(Addr, AM))
    return false;

  getAddressOperands(AM, Addr.getValueType(), Base, Disp, Index);
  return true;
}

SDNode *RISCVDAGToDAGISel::splitLargeImmediate(unsigned Opcode, SDNode *Node,
                                                 SDValue Op0, uint64_t UpperVal,
                                                 uint64_t LowerVal) {
  EVT VT = Node->getValueType(0);
  DebugLoc DL = Node->getDebugLoc();
  SDValue Upper = CurDAG->getConstant(UpperVal, VT);
  if (Op0.getNode())
    Upper = CurDAG->getNode(Opcode, DL, VT, Op0, Upper);
  Upper = SDValue(Select(Upper.getNode()), 0);

  SDValue Lower = CurDAG->getConstant(LowerVal, VT);
  SDValue Or = CurDAG->getNode(Opcode, DL, VT, Upper, Lower);
  return Or.getNode();
}

SDNode *RISCVDAGToDAGISel::Select(SDNode *Node) {
  // Dump information about the Node being selected
  DEBUG(errs() << "Selecting: "; Node->dump(CurDAG); errs() << "\n");

  // If we have a custom node, we already have selected!
  if (Node->isMachineOpcode()) {
    DEBUG(errs() << "== "; Node->dump(CurDAG); errs() << "\n");
    return 0;
  }

  unsigned Opcode = Node->getOpcode();
  switch (Opcode) {
  case ISD::OR:
  case ISD::XOR:
    // If this is a 64-bit operation in which both 32-bit halves are nonzero,
    // split the operation into two.
    if (Node->getValueType(0) == MVT::i64)
      if (ConstantSDNode *Op1 = dyn_cast<ConstantSDNode>(Node->getOperand(1))) {
        uint64_t Val = Op1->getZExtValue();
        if (!RISCV::isImmLF(Val) && !RISCV::isImmHF(Val))
          Node = splitLargeImmediate(Opcode, Node, Node->getOperand(0),
                                     Val - uint32_t(Val), uint32_t(Val));
      }
    break;

  case ISD::Constant:
    // If this is a 64-bit constant that is out of the range of LLILF,
    // LLIHF and LGFI, split it into two 32-bit pieces.
    if (Node->getValueType(0) == MVT::i64) {
      uint64_t Val = cast<ConstantSDNode>(Node)->getZExtValue();
      if (!RISCV::isImmLF(Val) && !RISCV::isImmHF(Val) && !isInt<32>(Val))
        Node = splitLargeImmediate(ISD::OR, Node, SDValue(),
                                   Val - uint32_t(Val), uint32_t(Val));
    }
    break;

  case ISD::ATOMIC_LOAD_SUB:
    // Try to convert subtractions of constants to additions.
    if (ConstantSDNode *Op2 = dyn_cast<ConstantSDNode>(Node->getOperand(2))) {
      uint64_t Value = -Op2->getZExtValue();
      EVT VT = Node->getValueType(0);
      if (VT == MVT::i32 || isInt<32>(Value)) {
        SDValue Ops[] = { Node->getOperand(0), Node->getOperand(1),
                          CurDAG->getConstant(int32_t(Value), VT) };
        Node = CurDAG->MorphNodeTo(Node, ISD::ATOMIC_LOAD_ADD,
                                   Node->getVTList(), Ops, array_lengthof(Ops));
      }
    }
    break;
  }

  // Select the default instruction
  SDNode *ResNode = SelectCode(Node);

  DEBUG(errs() << "=> ";
        if (ResNode == NULL || ResNode == Node)
          Node->dump(CurDAG);
        else
          ResNode->dump(CurDAG);
        errs() << "\n";
        );
  return ResNode;
}

bool RISCVDAGToDAGISel::
SelectInlineAsmMemoryOperand(const SDValue &Op,
                             char ConstraintCode,
                             std::vector<SDValue> &OutOps) {
  assert(ConstraintCode == 'm' && "Unexpected constraint code");
  // Accept addresses with short displacements, which are compatible
  // with Q, R, S and T.  But keep the index operand for future expansion.
  SDValue Base, Disp, Index;
  if (!selectBDXAddr(RISCVAddressingMode::FormBD,
                     RISCVAddressingMode::Disp12Only,
                     Op, Base, Disp, Index))
    return true;
  OutOps.push_back(Base);
  OutOps.push_back(Disp);
  OutOps.push_back(Index);
  return false;
}
