/* ********************************************************************************************* */
/* * RISC-V Vector Instruction Builder: Pattern match and substitution                         * */
/* * Author: André Bannwart Perina and Guilherme Bileki                                        * */
/* ********************************************************************************************* */
/* * Copyright (c) 2016 André B. Perina, Guilherme Bileki                                      * */
/* *                                                                                           * */
/* * Permission is hereby granted, free of charge, to any person obtaining a copy of this      * */
/* * software and associated documentation files (the "Software"), to deal with the Software   * */
/* * without restriction, including without limitation the rights to use, copy, modify, merge, * */
/* * publish, distribute, sublicense, and/or sell copies of the Software, and to permit        * */
/* * persons to whom the Software is furnished to do so, subject to the following conditions:  * */
/* *                                                                                           * */
/* * * Redistributions of source code must retain the above copyright notice, this list of     * */
/* *   conditions and the following disclaimers.                                               * */
/* * * Redistributions in binary form must reproduce the above copyright notice, this list of  * */
/* *   conditions and the following disclaimers in the documentation and/or other materials    * */
/* *   provided with the distribution.                                                         * */
/* * * Neither the names of LLVM Team, University of Illinois at Urbana-Champaign, nor the     * */
/* *   names of its contributors may be used to endorse or promote products derived from this  * */
/* *   Software without specific prior written permission.                                     * */
/* *                                                                                           * */
/* * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,       * */
/* * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR  * */
/* * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE   * */
/* * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT  * */
/* * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER * */
/* * DEALINGS WITH THE SOFTWARE.                                                               * */
/* ********************************************************************************************* */

#ifndef LLVM_LIB_TARGET_RISCV_RISCVVECTORINSTRBUILDER_H
#define LLVM_LIB_TARGET_RISCV_RISCVVECTORINSTRBUILDER_H

#include "RISCVAsmPrinter.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfoImpls.h"

/* Maximum available registers for one vector operation */
#define XVEC_AVAIL_REGS 28

namespace llvm {

class RISCVVectorInstrBuilder {
public:
	/**
	 * @brief Defines which class a pattern was matched to.
	 */
	enum MatchClass {
		RR,
		RI,
		IR
	};

private:
	/**
	 * @brief Vector with classes of each matched pattern.
	 */
	std::vector<enum MatchClass> classVec;

	/**
	 * @brief Vector with start point (first instruction within basic block) of each matched pattern.
	 *        Start point counts include pseudo instructions.
	 */
	std::vector<unsigned int> startPointVec;

	/**
	 * @brief Vector with block size of each matched pattern.
	 */
	std::vector<unsigned int> blockSizeVec;

	/**
	 * @brief Vector with operation opcode of each matched pattern.
	 */
	std::vector<unsigned int> opcodeVec;

	/**
	 * @brief Vector with identified immediates (first or second operands) of each matched pattern.
	 */
	std::vector<int> xImmVec;

	/**
	 * @brief Vector with index register (indexed load) for xA (first operand) of each matched pattern.
	 * @note For case IR, there's no such indexed load. Therefore -1 will be stored instead.
	 */
	std::vector<int> xAIdxVec;

	/**
	 * @brief Vector with index register (indexed load) for xB (second operand) of each matched pattern.
	 * @note For case RI, there's no such indexed load. Therefore -1 will be stored instead.
	 */
	std::vector<int> xBIdxVec;

	/**
	 * @brief Vector with index register (indexed store) for xA (result) of each matched pattern.
	 */
	std::vector<int> xCIdxVec;

	/**
	 * @brief Mapping of RV32I general-purpose registers.
	 */
	unsigned int rvRegs[32] = {
		RISCV::zero, RISCV::ra, RISCV::sp, RISCV::gp, RISCV::tp, RISCV::t0, RISCV::t1, RISCV::t2,
		RISCV::s0, RISCV::s1, RISCV::a0, RISCV::a1, RISCV::a2, RISCV::a3, RISCV::a4, RISCV::a5,
		RISCV::a6, RISCV::a7, RISCV::s2, RISCV::s3, RISCV::s4, RISCV::s5, RISCV::s6, RISCV::s7,
		RISCV::s8, RISCV::s9, RISCV::s10, RISCV::s11, RISCV::t3, RISCV::t4, RISCV::t5, RISCV::t6
	};

public:
	/**
	 * @brief Get how many matches were found.
	 *
	 * @return The number of found matches.
	 */
	unsigned int getListSize();

	/**
	 * @brief Get match class for given match.
	 *
	 * @param i Match position in vectors.
	 *
	 * @return A MatchClass value (RR, RI or IR).
	 */
	unsigned int getClassAt(unsigned int i);

	/**
	 * @brief Get match class for given match.
	 *
	 * @param i Match position in vectors.
	 *
	 * @return The start point.
	 */
	unsigned int getStartPointAt(unsigned int i);

	/**
	 * @brief Get block size for given match.
	 *
	 * @param i Match position in vectors.
	 *
	 * @return The block size.
	 */
	unsigned int getBlockSizeAt(unsigned int i);

	/**
	 * @brief Get opcode for given match.
	 *
	 * @param i Match position in vectors.
	 *
	 * @return The block size.
	 */
	unsigned int getOpcodeAt(unsigned int i);

	/**
	 * @brief Get equivalent vector opcode for given match. Example: If matched opcode is ADDI,
	 *        this method will return ADDIV.
	 *
	 * @param i Match position in vectors.
	 *
	 * @return The equivalent vector opcode.
	 */
	unsigned int getEqVectorOpcodeAt(unsigned int i);

	/**
	 * @brief Get xImm for given match.
	 *
	 * @param i Match position in vectors.
	 *
	 * @return xImm.
	 *
	 * @note See definition of @v xImmVec for further details about xImm.
	 */
	int getXImmAt(unsigned int i);

	/**
	 * @brief Get xAIdx for given match.
	 *
	 * @param i Match position in vectors.
	 *
	 * @return xAIdx.
	 *
	 * @note See definition of @v xAIdxVec for further details about xAIdx.
	 */
	int getXAIdxAt(unsigned int i);

	/**
	 * @brief Get xBIdx for given match.
	 *
	 * @param i Match position in vectors.
	 *
	 * @return xBIdx.
	 *
	 * @note See definition of @v xBIdxVec for further details about xBIdx.
	 */
	int getXBIdxAt(unsigned int i);

	/**
	 * @brief Get xCIdx for given match.
	 *
	 * @param i Match position in vectors.
	 *
	 * @return xCIdx.
	 *
	 * @note See definition of @v xCIdxVec for further details about xCIdx.
	 */
	int getXCIdxAt(unsigned int i);

	/**
	 * @brief Check a given basic block for patterns of class RR and appends them to the matches
	 *        vectors.
	 *
	 * @param MBB A reference to a MachineBasicBlock.
	 *
	 * @return true if matches were found, false otherwise.
	 */
	bool checkForVectorPatternRR(const MachineBasicBlock &MBB);

	/**
	 * @brief Check a given basic block for patterns of class RI and appends them to the matches
	 *        vectors.
	 *
	 * @param MBB A reference to a MachineBasicBlock.
	 *
	 * @return true if matches were found, false otherwise.
	 */
	bool checkForVectorPatternRI(const MachineBasicBlock &MBB);

	/**
	 * @brief Check a given basic block for patterns of class IR and appends them to the matches
	 *        vectors.
	 *
	 * @param MBB A reference to a MachineBasicBlock.
	 *
	 * @return true if matches were found, false otherwise.
	 */
	bool checkForVectorPatternIR(const MachineBasicBlock &MBB);

	/**
	 * @brief Substitute all present matches in the matches vectors.
	 *
	 * @param MBB The MachineBasicBlock where substitutions will be performed.
	 * @param Subtarget A RISCVSubtarget description. Using @v Subtarget attribute from
	 *        RISCVAsmPrinter should suffice.
	 */
	void substituteAllMatches(MachineBasicBlock *MBB, const RISCVSubtarget *Subtarget);
};

}

#endif
