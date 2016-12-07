#ifndef LLVM_LIB_TARGET_RISCV_RISCVVECTORINSTRBUILDER_H
#define LLVM_LIB_TARGET_RISCV_RISCVVECTORINSTRBUILDER_H

#include "RISCVAsmPrinter.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfoImpls.h"

namespace llvm {

class RISCVVectorInstrBuilder {
public:
	enum MatchClass {
		RR,
		RI,
		IR
	};

private:
	std::vector<enum MatchClass> classVec;
	std::vector<unsigned int> startPointVec;
	std::vector<unsigned int> blockSizeVec;
	std::vector<unsigned int> opcodeVec;
	std::vector<int> xAVec;
	std::vector<int> xBVec;
	std::vector<int> xAIdxVec;
	std::vector<int> xBIdxVec;
	std::vector<int> xCIdxVec;
	unsigned int rvRegs[32] = {
		RISCV::zero, RISCV::ra, RISCV::sp, RISCV::gp, RISCV::tp, RISCV::t0, RISCV::t1, RISCV::t2,
		RISCV::s0, RISCV::s1, RISCV::a0, RISCV::a1, RISCV::a2, RISCV::a3, RISCV::a4, RISCV::a5,
		RISCV::a7, RISCV::a7, RISCV::s2, RISCV::s3, RISCV::s4, RISCV::s5, RISCV::s6, RISCV::s7,
		RISCV::s8, RISCV::s9, RISCV::s10, RISCV::s11, RISCV::t3, RISCV::t4, RISCV::t5, RISCV::t6
	};

public:
	unsigned int getListSize();
	unsigned int getClassAt(unsigned int i);
	unsigned int getStartPointAt(unsigned int i);
	unsigned int getBlockSizeAt(unsigned int i);
	unsigned int getOpcodeAt(unsigned int i);
	unsigned int getEqVectorOpcodeAt(unsigned int i);
	int getXAAt(unsigned int i);
	int getXBAt(unsigned int i);
	int getXAIdxAt(unsigned int i);
	int getXBIdxAt(unsigned int i);
	int getXCIdxAt(unsigned int i);
	bool checkForVectorPatternRR(const MachineBasicBlock &MBB);
	bool checkForVectorPatternRI(const MachineBasicBlock &MBB);
	bool checkForVectorPatternIR(const MachineBasicBlock &MBB);
	bool substituteAllMatches(MachineBasicBlock *MBB, const RISCVSubtarget *Subtarget);
};

} // end namespace llvm

#endif
