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

#include "RISCVVectorInstrBuilder.h"

// TODO: Remove
#include <iostream>

using namespace llvm;

/**
 * @brief Get how many matches were found.
 */
unsigned int RISCVVectorInstrBuilder::getListSize() {
	return classVec.size();
}

/**
 * @brief Get match class for given match.
 */
unsigned int RISCVVectorInstrBuilder::getClassAt(unsigned int i) {
	return classVec[i];
}

/**
 * @brief Get match class for given match.
 */
unsigned int RISCVVectorInstrBuilder::getStartPointAt(unsigned int i) {
	return startPointVec[i];
}

/**
 * @brief Get block size for given match.
 */
unsigned int RISCVVectorInstrBuilder::getBlockSizeAt(unsigned int i) {
	return blockSizeVec[i];
}

/**
 * @brief Get opcode for given match.
 */
unsigned int RISCVVectorInstrBuilder::getOpcodeAt(unsigned int i) {
	return opcodeVec[i];
}

/**
 * @brief Get equivalent vector opcode for given match. Example: If matched opcode is ADDI,
 *        this method will return ADDIV.
 */
unsigned int RISCVVectorInstrBuilder::getEqVectorOpcodeAt(unsigned int i) {
	switch(opcodeVec[i]) {
		case RISCV::ADD:
			return RISCV::ADDV;
		case RISCV::SUB:
			return RISCV::SUBV;
		case RISCV::SLL:
			return RISCV::SLLV;
		case RISCV::SLT:
			return RISCV::SLTV;
		case RISCV::SLTU:
			return RISCV::SLTUV;
		case RISCV::XOR:
			return RISCV::XORV;
		case RISCV::SRL:
			return RISCV::SRLV;
		case RISCV::SRA:
			return RISCV::SRAV;
		case RISCV::OR:
			return RISCV::ORV;
		case RISCV::AND:
			return RISCV::ANDV;
		case RISCV::ADDI:
			return RISCV::ADDIV;
		case RISCV::SLTI:
			return RISCV::SLTIV;
		case RISCV::SLTIU:
			return RISCV::SLTIUV;
		case RISCV::XORI:
			return RISCV::XORIV;
		case RISCV::ORI:
			return RISCV::ORIV;
		case RISCV::ANDI:
			return RISCV::ANDIV;
		case RISCV::SLLI:
			return RISCV::SLLIV;
		case RISCV::SRLI:
			return RISCV::SRLIV;
		case RISCV::SRAI:
			return RISCV::SRAIV;
		default:
			return opcodeVec[i];
	}
}

/**
 * @brief Get xImm for given match.
 */
int RISCVVectorInstrBuilder::getXImmAt(unsigned int i) {
	return xImmVec[i];
}

/**
 * @brief Get xAIdx for given match.
 */
int RISCVVectorInstrBuilder::getXAIdxAt(unsigned int i) {
	return xAIdxVec[i];
}

/**
 * @brief Get xBIdx for given match.
 */
int RISCVVectorInstrBuilder::getXBIdxAt(unsigned int i) {
	return xBIdxVec[i];
}

/**
 * @brief Get xCIdx for given match.
 */
int RISCVVectorInstrBuilder::getXCIdxAt(unsigned int i) {
	return xCIdxVec[i];
}

/**
 * @brief Check a given basic block for patterns of class RR and appends them to the matches
 *        vectors.
 */
bool RISCVVectorInstrBuilder::checkForVectorPatternRR(const MachineBasicBlock &MBB) {
	bool rv = false;
	unsigned int matchState = 0;
	unsigned int point = 0;
	bool overrideSlide = false;
	unsigned int startPoint;
	unsigned int opcode;
	unsigned int offset;
	int xAIdx;
	int xBIdx;
	int xCIdx;

	/* Since this iterator is not bidirectional, we do this magic to have 4 instructions in hand */
	auto MI = MBB.begin();
	if(MI == MBB.end())
		return false;
	auto MI0 = MI++;
	if(MI == MBB.end())
		return false;
	auto MI1 = MI++;
	if(MI == MBB.end())
		return false;
	auto MI2 = MI++;
	if(MI == MBB.end())
		return false;
	auto MI3 = MI++;

	/* Iterate through all instructions */
	while(true) {
		unsigned int iOpcode;
		unsigned int iOffset;
		int ixA;
		int ixB;
		int ixC;
		int ixAIdx;
		int ixBIdx;
		int ixCIdx;
		bool operandsConsistent;
		bool instructionsMatch;
		bool registersMatch;
		bool registersAreDifferent;
		bool offsetsMatch;

		/* Pattern state machine */
		switch(matchState) {
			/* State 0: Check for first block (LW; LW; OP; SW) */
			case 0:
				/**
				 * Check if all instructions are consistent regarding number of operands and their
				 * types. If instructions are consistent, we can then check if everything fits.
				 */
				if(
					(3 == MI0->getNumOperands()) &&
					(3 == MI1->getNumOperands()) &&
					(3 == MI2->getNumOperands()) &&
					(3 == MI3->getNumOperands()) &&
					(MI0->getOperand(0).isReg() && MI0->getOperand(1).isImm() && MI0->getOperand(2).isReg()) &&
					(MI1->getOperand(0).isReg() && MI1->getOperand(1).isImm() && MI1->getOperand(2).isReg()) &&
					(MI2->getOperand(0).isReg() && MI2->getOperand(1).isReg() && MI2->getOperand(2).isReg()) &&
					(MI3->getOperand(0).isReg() && MI3->getOperand(1).isImm() && MI3->getOperand(2).isReg())
				) {
					operandsConsistent = true;

					/* Get arith opcode and operands for this block */
					iOpcode = MI2->getOpcode();
					iOffset = MI0->getOperand(1).getImm();
					ixA = MI0->getOperand(0).getReg();
					ixB = MI1->getOperand(0).getReg();
					ixC = MI2->getOperand(0).getReg();
					ixAIdx = MI0->getOperand(2).getReg();
					ixBIdx = MI1->getOperand(2).getReg();
					ixCIdx = MI3->getOperand(2).getReg();

					/* Check if instructions match (LW; LW; ADD/SUB/...; SW) */
					instructionsMatch =	(
											(RISCV::LW == MI0->getOpcode()) &&
											(RISCV::LW == MI1->getOpcode()) &&
											(
												(RISCV::ADD == iOpcode) ||
												(RISCV::SUB == iOpcode) ||
												(RISCV::SLL == iOpcode) ||
												(RISCV::SLT == iOpcode) ||
												(RISCV::SLTU == iOpcode) ||
												(RISCV::XOR == iOpcode) ||
												(RISCV::SRL == iOpcode) ||
												(RISCV::SRA == iOpcode) ||
												(RISCV::OR == iOpcode) ||
												(RISCV::AND == iOpcode)
											) &&
											(RISCV::SW == MI3->getOpcode())
										);

					/* Check if registers match */
					registersMatch =	(
											(
												(
													((unsigned int) ixA == MI2->getOperand(1).getReg()) &&
													((unsigned int) ixB == MI2->getOperand(2).getReg())
												) ||
												(
													((unsigned int) ixB == MI2->getOperand(1).getReg()) &&
													((unsigned int) ixA == MI2->getOperand(2).getReg())
												)
											) &&
											((unsigned int) ixC == MI3->getOperand(0).getReg())
										);

					/**
					 * Check if found registers are different among them.
					 * Note: ixA or ixB may be equal to ixC
					 */
					registersAreDifferent =	(
												(ixA != ixB) &&
												(ixA != ixAIdx) &&
												(ixA != ixBIdx) &&
												(ixA != ixCIdx) &&
												(ixB != ixAIdx) &&
												(ixB != ixBIdx) &&
												(ixB != ixCIdx) &&
												(ixC != ixAIdx) &&
												(ixC != ixBIdx) &&
												(ixC != ixCIdx) &&
												(ixAIdx != ixBIdx) &&
												(ixAIdx != ixCIdx) &&
												(ixBIdx != ixCIdx)
											);

					/* Check if address offset matches */
					offsetsMatch =	(
										(0 == iOffset) &&
										(iOffset == MI1->getOperand(1).getImm()) &&
										(iOffset == MI3->getOperand(1).getImm())
									);
				}
				else {
					operandsConsistent = false;
				}

				/* If all matches are true, we found a block! */
				if(operandsConsistent && instructionsMatch && registersMatch && registersAreDifferent && offsetsMatch) {
					/* Save information */
					startPoint = point;
					opcode = iOpcode;
					offset = iOffset;
					xAIdx = ixAIdx;
					xBIdx = ixBIdx;
					xCIdx = ixCIdx;
	
					/* Head to next state */
					matchState = 1;
				}
				break;
			/* State 1: Jump a block of instructions */
			case 1:
				/**
				 * Since we already found the first block with 4 valid instructions, we need to
				 * slide over 4 instructions before analysing again.
				 */
				if(4 == (point - startPoint)) {
					overrideSlide = true;
					matchState = 2;
				}
				break;
			/**
			 * State 2: Check for non-first blocks (LW; LW, OP; SW).
			 * Note: There is little difference in this state when compared to state 0
			 */
			case 2:
				/**
				 * Check if all instructions are consistent regarding number of operands and their
				 * types. If instructions are consistent, we can then check if everything fits.
				 */
				if(
					(3 == MI0->getNumOperands()) &&
					(3 == MI1->getNumOperands()) &&
					(3 == MI2->getNumOperands()) &&
					(3 == MI3->getNumOperands()) &&
					(MI0->getOperand(0).isReg() && MI0->getOperand(1).isImm() && MI0->getOperand(2).isReg()) &&
					(MI1->getOperand(0).isReg() && MI1->getOperand(1).isImm() && MI1->getOperand(2).isReg()) &&
					(MI2->getOperand(0).isReg() && MI2->getOperand(1).isReg() && MI2->getOperand(2).isReg()) &&
					(MI3->getOperand(0).isReg() && MI3->getOperand(1).isImm() && MI3->getOperand(2).isReg())
				) {
					operandsConsistent = true;

					/* Get arith opcode and operands for this block */
					iOpcode = MI2->getOpcode();
					iOffset = MI0->getOperand(1).getImm();
					ixA = MI0->getOperand(0).getReg();
					ixB = MI1->getOperand(0).getReg();
					ixC = MI2->getOperand(0).getReg();
					ixAIdx = MI0->getOperand(2).getReg();
					ixBIdx = MI1->getOperand(2).getReg();
					ixCIdx = MI3->getOperand(2).getReg();

					/* Check if instructions match (LW; LW; ADD/SUB/...; SW) */
					instructionsMatch =	(
											(RISCV::LW == MI0->getOpcode()) &&
											(RISCV::LW == MI1->getOpcode()) &&
											(
												(RISCV::ADD == iOpcode) ||
												(RISCV::SUB == iOpcode) ||
												(RISCV::SLL == iOpcode) ||
												(RISCV::SLT == iOpcode) ||
												(RISCV::SLTU == iOpcode) ||
												(RISCV::XOR == iOpcode) ||
												(RISCV::SRL == iOpcode) ||
												(RISCV::SRA == iOpcode) ||
												(RISCV::OR == iOpcode) ||
												(RISCV::AND == iOpcode)
											) &&
											(RISCV::SW == MI3->getOpcode())
										);

					/* Check if registers match */
					registersMatch =	(
											(
												(
													((unsigned int) ixA == MI2->getOperand(1).getReg()) &&
													((unsigned int) ixB == MI2->getOperand(2).getReg())
												) ||
												(
													((unsigned int) ixB == MI2->getOperand(1).getReg()) &&
													((unsigned int) ixA == MI2->getOperand(2).getReg())
												)
											) &&
											((unsigned int) ixC == MI3->getOperand(0).getReg())
										);

					/**
					 * Check if found registers are different among them.
					 * Note: ixA or ixB may be equal to ixC
					 */
					registersAreDifferent =	(
												(ixA != ixB) &&
												(ixA != ixAIdx) &&
												(ixA != ixBIdx) &&
												(ixA != ixCIdx) &&
												(ixB != ixAIdx) &&
												(ixB != ixBIdx) &&
												(ixB != ixCIdx) &&
												(ixC != ixAIdx) &&
												(ixC != ixBIdx) &&
												(ixC != ixCIdx) &&
												(ixAIdx != ixBIdx) &&
												(ixAIdx != ixCIdx) &&
												(ixBIdx != ixCIdx)
											);

					/* Check if address offset matches */
					offsetsMatch =	(
										(iOffset == MI1->getOperand(1).getImm()) &&
										(iOffset == MI3->getOperand(1).getImm())
									);
				}
				else {
					operandsConsistent = false;
				}

				/* If all matches are true, we found a block! */
				if(operandsConsistent && instructionsMatch && registersMatch && registersAreDifferent && offsetsMatch) {
					/* Update offset */
					offset += 4;

					/* If any of these differs, it means that this match is over */
					if(
						(opcode != iOpcode) ||
						(xAIdx != ixAIdx) || (xBIdx != ixBIdx) || (xCIdx != ixCIdx) ||
						(offset != iOffset)
					) {
						/* We're finished with this match. Save information to the lists */
						classVec.push_back(MatchClass::RR);
						startPointVec.push_back(startPoint);
						blockSizeVec.push_back((point - startPoint) / 4);
						opcodeVec.push_back(opcode);
						xImmVec.push_back(-1);
						xAIdxVec.push_back(xAIdx);
						xBIdxVec.push_back(xBIdx);
						xCIdxVec.push_back(xCIdx);

						rv = true;
						overrideSlide = true;
						matchState = 0;
					}
					else {
						matchState = 3;
					}
				}
				else {
					/* We're finished with this match. Save information to the lists */
					classVec.push_back(MatchClass::RR);
					startPointVec.push_back(startPoint);
					blockSizeVec.push_back((point - startPoint) / 4);
					opcodeVec.push_back(opcode);
					xImmVec.push_back(-1);
					xAIdxVec.push_back(xAIdx);
					xBIdxVec.push_back(xBIdx);
					xCIdxVec.push_back(xCIdx);

					rv = true;
					overrideSlide = true;
					matchState = 0;
				}
				break;
			/* State 3: Jump a block of instructions */
			case 3:
				/**
				 * Since we found a block with 4 valid instructions, we need to slide over 4
				 * instructions before analysing again.
				 */
				if(!((point - startPoint) % 4)) {
					overrideSlide = true;
					matchState = 2;
				}
				break;
		}

		/* Slide window (if no override was activated) */
		if(!overrideSlide) {
			MI0 = MI1;
			MI1 = MI2;
			MI2 = MI3;
			if(MI == MBB.end())
				return rv;
			MI3 = MI++;

			point++;
		}
		else {
			overrideSlide = false;
		}
	}

	return rv;
}

/**
 * @brief Check a given basic block for patterns of class RI and appends them to the matches
 *        vectors.
 */
bool RISCVVectorInstrBuilder::checkForVectorPatternRI(const MachineBasicBlock &MBB) {
	bool rv = false;
	unsigned int matchState = 0;
	unsigned int point = 0;
	bool overrideSlide = false;
	unsigned int startPoint;
	unsigned int opcode;
	unsigned int offset;
	int xImm;
	int xAIdx;
	int xCIdx;

	/* Since this iterator is not bidirectional, we do this magic to have 3 instructions in hand */
	auto MI = MBB.begin();
	if(MI == MBB.end())
		return false;
	auto MI0 = MI++;
	if(MI == MBB.end())
		return false;
	auto MI1 = MI++;
	if(MI == MBB.end())
		return false;
	auto MI2 = MI++;

	/* Iterate through all instructions */
	while(true) {
		unsigned int iOpcode;
		unsigned int iOffset;
		int ixA;
		int ixImm;
		int ixC;
		int ixAIdx;
		int ixCIdx;
		bool operandsConsistent;
		bool instructionsMatch;
		bool registersMatch;
		bool registersAreDifferent;
		bool offsetsMatch;

		/* Pattern state machine */
		switch(matchState) {
			/* State 0: Check for first block (LW; OPI; SW) */
			case 0:
				/**
				 * Check if all instructions are consistent regarding number of operands and their
				 * types. If instructions are consistent, we can then check if everything fits.
				 */
				if(
					(3 == MI0->getNumOperands()) &&
					(3 == MI1->getNumOperands()) &&
					(3 == MI2->getNumOperands()) &&
					(MI0->getOperand(0).isReg() && MI0->getOperand(1).isImm() && MI0->getOperand(2).isReg()) &&
					(MI1->getOperand(0).isReg() && MI1->getOperand(1).isReg() && MI1->getOperand(2).isImm()) &&
					(MI2->getOperand(0).isReg() && MI2->getOperand(1).isImm() && MI2->getOperand(2).isReg())
				) {
					operandsConsistent = true;

					/* Get arith opcode and operands for this block */
					iOpcode = MI1->getOpcode();
					iOffset = MI0->getOperand(1).getImm();
					ixA = MI0->getOperand(0).getReg();
					ixImm = MI1->getOperand(2).getImm();
					ixC = MI1->getOperand(0).getReg();
					ixAIdx = MI0->getOperand(2).getReg();
					ixCIdx = MI2->getOperand(2).getReg();

					/* Check if instructions match (LW; ADDI/SUBI/...; SW) */
					instructionsMatch =	(
											(RISCV::LW == MI0->getOpcode()) &&
											(
												(RISCV::ADDI == iOpcode) ||
												(RISCV::SLTI == iOpcode) ||
												(RISCV::SLTIU == iOpcode) ||
												(RISCV::XORI == iOpcode) ||
												(RISCV::ORI == iOpcode) ||
												(RISCV::ANDI == iOpcode) ||
												(RISCV::SLLI == iOpcode) ||
												(RISCV::SRLI == iOpcode) ||
												(RISCV::SRAI == iOpcode)
											) &&
											(RISCV::SW == MI2->getOpcode())
										);

					/* Check if registers match */
					registersMatch =	(
											((unsigned int) ixA == MI1->getOperand(1).getReg()) &&
											((unsigned int) ixC == MI2->getOperand(0).getReg())
										);

					/**
					 * Check if found registers are different among them.
					 * Note: ixA may be equal to ixC
					 */
					registersAreDifferent =	(
												(ixA != ixAIdx) &&
												(ixA != ixCIdx) &&
												(ixAIdx != ixCIdx)
											);

					/* Check if address offset matches */
					offsetsMatch =	(
										(0 == iOffset) &&
										(iOffset == MI2->getOperand(1).getImm())
									);
				}
				else {
					operandsConsistent = false;
				}

			
				/* If all matches are true, we found a block! */
				if(operandsConsistent && instructionsMatch && registersMatch && registersAreDifferent && offsetsMatch) {
					/* Save information */
					startPoint = point;
					opcode = iOpcode;
					offset = iOffset;
					xImm = ixImm;
					xAIdx = ixAIdx;
					xCIdx = ixCIdx;
	
					/* Head to next state */
					matchState = 1;
				}
				break;
			/* State 1: Jump a block of instructions */
			case 1:
				/**
				 * Since we already found the first block with 3 valid instructions, we need to
				 * slide over 3 instructions before analysing again.
				 */
				if(3 == (point - startPoint)) {
					overrideSlide = true;
					matchState = 2;
				}
				break;
			/**
			 * State 2: Check for non-first blocks (LW; OPI; SW).
			 * Note: There is little difference in this state when compared to state 0.
			 */
			case 2:
				/**
				 * Check if all instructions are consistent regarding number of operands and their
				 * types. If instructions are consistent, we can then check if everything fits.
				 */
				if(
					(3 == MI0->getNumOperands()) &&
					(3 == MI1->getNumOperands()) &&
					(3 == MI2->getNumOperands()) &&
					(MI0->getOperand(0).isReg() && MI0->getOperand(1).isImm() && MI0->getOperand(2).isReg()) &&
					(MI1->getOperand(0).isReg() && MI1->getOperand(1).isReg() && MI1->getOperand(2).isImm()) &&
					(MI2->getOperand(0).isReg() && MI2->getOperand(1).isImm() && MI2->getOperand(2).isReg())
				) {
					operandsConsistent = true;

					/* Get arith opcode and operands for this block */
					iOpcode = MI1->getOpcode();
					iOffset = MI0->getOperand(1).getImm();
					ixA = MI0->getOperand(0).getReg();
					ixImm = MI1->getOperand(2).getImm();
					ixC = MI1->getOperand(0).getReg();
					ixAIdx = MI0->getOperand(2).getReg();
					ixCIdx = MI2->getOperand(2).getReg();

					/* Check if instructions match (LW; ADDI/SUBI/...; SW) */
					instructionsMatch =	(
											(RISCV::LW == MI0->getOpcode()) &&
											(
												(RISCV::ADDI == iOpcode) ||
												(RISCV::SLTI == iOpcode) ||
												(RISCV::SLTIU == iOpcode) ||
												(RISCV::XORI == iOpcode) ||
												(RISCV::ORI == iOpcode) ||
												(RISCV::ANDI == iOpcode) ||
												(RISCV::SLLI == iOpcode) ||
												(RISCV::SRLI == iOpcode) ||
												(RISCV::SRAI == iOpcode)
											) &&
											(RISCV::SW == MI2->getOpcode())
										);

					/* Check if registers match */
					registersMatch =	(
											((unsigned int) ixA == MI1->getOperand(0).getReg()) &&
											((unsigned int) ixC == MI2->getOperand(0).getReg())
										);

					/**
					 * Check if found registers are different among them.
					 * Note: ixA or ixB may be equal to ixC
					 */
					registersAreDifferent =	(
												(ixA != ixAIdx) &&
												(ixA != ixCIdx) &&
												(ixAIdx != ixCIdx)
											);

					/* Check if address offset matches */
					offsetsMatch =	(
										(iOffset == MI2->getOperand(1).getImm())
									);
				}
				else {
					operandsConsistent = false;
				}

				/* If all matches are true, we found a block! */
				if(operandsConsistent && instructionsMatch && registersMatch && registersAreDifferent && offsetsMatch) {
					/* Update offset */
					offset += 4;

					/* If any of these differs, it means that this match is over */
					if(
						(opcode != iOpcode) ||
						(xImm != ixImm) ||
						(xAIdx != ixAIdx) || (xCIdx != ixCIdx) ||
						(offset != iOffset)
					) {
						/* We're finished with this match. Save information to the lists */
						classVec.push_back(MatchClass::RI);
						startPointVec.push_back(startPoint);
						blockSizeVec.push_back((point - startPoint) / 3);
						opcodeVec.push_back(opcode);
						xImmVec.push_back(xImm);
						xAIdxVec.push_back(xAIdx);
						xBIdxVec.push_back(-1);
						xCIdxVec.push_back(xCIdx);

						rv = true;
						overrideSlide = true;
						matchState = 0;
					}
					else {
						matchState = 3;
					}
				}
				else {
					/* We're finished with this match. Save information to the lists */
					classVec.push_back(MatchClass::RI);
					startPointVec.push_back(startPoint);
					blockSizeVec.push_back((point - startPoint) / 3);
					opcodeVec.push_back(opcode);
					xImmVec.push_back(xImm);
					xAIdxVec.push_back(xAIdx);
					xBIdxVec.push_back(-1);
					xCIdxVec.push_back(xCIdx);

					rv = true;
					overrideSlide = true;
					matchState = 0;
				}
				break;
			/* State 3: Jump a block of instructions */
			case 3:
				/**
				 * Since we found a block with 3 valid instructions, we need to slide over 3
				 * instructions before analysing again.
				 */
				if(!((point - startPoint) % 3)) {
					overrideSlide = true;
					matchState = 2;
				}
				break;
		}

		/* Slide window (if no override was activated) */
		if(!overrideSlide) {
			MI0 = MI1;
			MI1 = MI2;
			if(MI == MBB.end())
				return rv;
			MI2 = MI++;

			point++;
		}
		else {
			overrideSlide = false;
		}
	}

	return rv;
}

/**
 * @brief Check a given basic block for patterns of class IR and appends them to the matches
 *        vectors.
 */
bool RISCVVectorInstrBuilder::checkForVectorPatternIR(const MachineBasicBlock &MBB) {
	bool rv = false;
	unsigned int matchState = 0;
	unsigned int point = 0;
	bool overrideSlide = false;
	unsigned int startPoint;
	unsigned int opcode;
	unsigned int offset;
	int xA;
	int xImm;
	int xBIdx;
	int xCIdx;

	/* Since this iterator is not bidirectional, we do this magic to have 4 instructions in hand */
	auto MI = MBB.begin();
	if(MI == MBB.end())
		return false;
	auto MI0 = MI++;
	if(MI == MBB.end())
		return false;
	auto MI1 = MI++;
	if(MI == MBB.end())
		return false;
	auto MI2 = MI++;
	if(MI == MBB.end())
		return false;
	auto MI3 = MI++;

	/* Iterate through all instructions */
	while(true) {
		unsigned int iOpcode;
		unsigned int iOffset;
		int ixA;
		int ixImm;
		int ixB;
		int ixC;
		int ixBIdx;
		int ixCIdx;
		bool operandsConsistent;
		bool instructionsMatch;
		bool registersMatch;
		bool registersAreDifferent;
		bool offsetsMatch;

		/* Pattern state machine */
		switch(matchState) {
			/* State 0: Check for first block (LW; LI; OP; SW) */
			case 0:
				/**
				 * Check if all instructions are consistent regarding number of operands and their
				 * types. If instructions are consistent, we can then check if everything fits.
				 */
				if(
					(3 == MI0->getNumOperands()) &&
					(2 == MI1->getNumOperands()) &&
					(3 == MI2->getNumOperands()) &&
					(3 == MI3->getNumOperands()) &&
					(MI0->getOperand(0).isReg() && MI0->getOperand(1).isImm() && MI0->getOperand(2).isReg()) &&
					(MI1->getOperand(0).isReg() && MI1->getOperand(1).isImm()) &&
					(MI2->getOperand(0).isReg() && MI2->getOperand(1).isReg() && MI2->getOperand(2).isReg()) &&
					(MI3->getOperand(0).isReg() && MI3->getOperand(1).isImm() && MI3->getOperand(2).isReg())
				) {
					operandsConsistent = true;

					/* Get arith opcode and operands for this block */
					iOpcode = MI2->getOpcode();
					iOffset = MI0->getOperand(1).getImm();
					ixA = MI1->getOperand(0).getReg();
					ixImm = MI1->getOperand(1).getImm();
					ixB = MI0->getOperand(0).getReg();
					ixC = MI2->getOperand(0).getReg();
					ixBIdx = MI0->getOperand(2).getReg();
					ixCIdx = MI3->getOperand(2).getReg();

					/* Check if instructions match (LW; LI; ADD/SUB/...; SW) */
					instructionsMatch =	(
											(RISCV::LW == MI0->getOpcode()) &&
											(RISCV::LI == MI1->getOpcode()) &&
											(
												(RISCV::ADD == iOpcode) ||
												(RISCV::SUB == iOpcode) ||
												(RISCV::SLL == iOpcode) ||
												(RISCV::SLT == iOpcode) ||
												(RISCV::SLTU == iOpcode) ||
												(RISCV::XOR == iOpcode) ||
												(RISCV::SRL == iOpcode) ||
												(RISCV::SRA == iOpcode) ||
												(RISCV::OR == iOpcode) ||
												(RISCV::AND == iOpcode)
											) &&
											(RISCV::SW == MI3->getOpcode())
										);

					/* Check if registers match */
					registersMatch =	(
											((unsigned int) ixA == MI2->getOperand(1).getReg()) &&
											((unsigned int) ixB == MI2->getOperand(2).getReg()) &&
											((unsigned int) ixC == MI3->getOperand(0).getReg())
										);

					/**
					 * Check if found registers are different among them.
					 * Note: ixA or ixB may be equal to ixC
					 */
					registersAreDifferent =	(
												(ixA != ixB) &&
												(ixA != ixBIdx) &&
												(ixA != ixCIdx) &&
												(ixB != ixBIdx) &&
												(ixB != ixCIdx) &&
												(ixC != ixBIdx) &&
												(ixC != ixCIdx) &&
												(ixBIdx != ixCIdx)
											);

					/* Check if address offset matches */
					offsetsMatch =	(
										(0 == iOffset) &&
										(iOffset == MI3->getOperand(1).getImm())
									);
				}
				else {
					operandsConsistent = false;
				}

				/* If all matches are true, we found a block! */
				if(operandsConsistent && instructionsMatch && registersMatch && registersAreDifferent && offsetsMatch) {
					/* Save information */
					startPoint = point;
					opcode = iOpcode;
					offset = iOffset;
					xA = ixA;
					xImm = ixImm;
					xBIdx = ixBIdx;
					xCIdx = ixCIdx;
	
					/* Head to next state */
					matchState = 1;
				}
				break;
			/* State 1: Jump a block of instructions */
			case 1:
				/**
				 * Since we already found the first block with 4 valid instructions, we need to
				 * slide over 4 instructions before analysing again.
				 */
				if(4 == (point - startPoint)) {
					overrideSlide = true;
					matchState = 2;
				}
				break;
			/* State 2: Check for non-first blocks (LW; OP; SW) */
			case 2:
				/**
				 * Check if all instructions are consistent regarding number of operands and their
				 * types. If instructions are consistent, we can then check if everything fits.
				 */
				if(
					(3 == MI0->getNumOperands()) &&
					(3 == MI1->getNumOperands()) &&
					(3 == MI2->getNumOperands()) &&
					(MI0->getOperand(0).isReg() && MI0->getOperand(1).isImm() && MI0->getOperand(2).isReg()) &&
					(MI1->getOperand(0).isReg() && MI1->getOperand(1).isReg() && MI1->getOperand(2).isReg()) &&
					(MI2->getOperand(0).isReg() && MI2->getOperand(1).isImm() && MI2->getOperand(2).isReg())
				) {
					operandsConsistent = true;

					/* Get arith opcode and operands for this block */
					iOpcode = MI1->getOpcode();
					iOffset = MI0->getOperand(1).getImm();
					ixA = MI1->getOperand(1).getReg();
					ixB = MI0->getOperand(0).getReg();
					ixC = MI1->getOperand(0).getReg();
					ixBIdx = MI0->getOperand(2).getReg();
					ixCIdx = MI2->getOperand(2).getReg();

					/* Check if instructions match (LW; ADD/SUB/...; SW) */
					instructionsMatch =	(
											(RISCV::LW == MI0->getOpcode()) &&
											(
												(RISCV::ADD == iOpcode) ||
												(RISCV::SUB == iOpcode) ||
												(RISCV::SLL == iOpcode) ||
												(RISCV::SLT == iOpcode) ||
												(RISCV::SLTU == iOpcode) ||
												(RISCV::XOR == iOpcode) ||
												(RISCV::SRL == iOpcode) ||
												(RISCV::SRA == iOpcode) ||
												(RISCV::OR == iOpcode) ||
												(RISCV::AND == iOpcode)
											) &&
											(RISCV::SW == MI2->getOpcode())
										);

					/* Check if registers match */
					registersMatch =	(
											((unsigned int) ixA == MI1->getOperand(1).getReg()) &&
											((unsigned int) ixB == MI1->getOperand(2).getReg()) &&
											((unsigned int) ixC == MI2->getOperand(0).getReg())
										);

					/**
					 * Check if found registers are different among them.
					 * Note: ixA or ixB may be equal to ixC
					 */
					registersAreDifferent =	(
												(ixA != ixB) &&
												(ixA != ixBIdx) &&
												(ixA != ixCIdx) &&
												(ixB != ixBIdx) &&
												(ixB != ixCIdx) &&
												(ixC != ixBIdx) &&
												(ixC != ixCIdx) &&
												(ixBIdx != ixCIdx)
											);

					/* Check if address offset matches */
					offsetsMatch =	(
										(iOffset == MI2->getOperand(1).getImm())
									);
				}
				else {
					operandsConsistent = false;
				}

				/* If all matches are true, we found a block! */
				if(operandsConsistent && instructionsMatch && registersMatch && registersAreDifferent && offsetsMatch) {
					/* Update offset */
					offset += 4;

					/* If any of these differs, it means that this match is over */
					if(
						(opcode != iOpcode) ||
						(xA != ixA) ||
						(xBIdx != ixBIdx) || (xCIdx != ixCIdx) ||
						(offset != iOffset)
					) {
						/* We're finished with this match. Save information to the lists */
						classVec.push_back(MatchClass::IR);
						startPointVec.push_back(startPoint);
						blockSizeVec.push_back(((point - (startPoint + 4)) / 3) + 1);
						opcodeVec.push_back(opcode);
						xImmVec.push_back(xImm);
						xAIdxVec.push_back(-1);
						xBIdxVec.push_back(xBIdx);
						xCIdxVec.push_back(xCIdx);

						rv = true;
						overrideSlide = true;
						matchState = 0;
					}
					else {
						matchState = 3;
					}
				}
				else {
					/* We're finished with this match. Save information to the lists */
					classVec.push_back(MatchClass::IR);
					startPointVec.push_back(startPoint);
					blockSizeVec.push_back(((point - (startPoint + 4)) / 3) + 1);
					opcodeVec.push_back(opcode);
					xImmVec.push_back(xImm);
					xAIdxVec.push_back(-1);
					xBIdxVec.push_back(xBIdx);
					xCIdxVec.push_back(xCIdx);

					rv = true;
					overrideSlide = true;
					matchState = 0;
				}
				break;
			/* State 3: Jump a block of instructions */
			case 3:
				/**
				 * Since we found a block with 3 valid instructions, we need to slide over 3
				 * instructions before analysing again.
				 */
				if(!((point - (startPoint + 4)) % 3)) {
					overrideSlide = true;
					matchState = 2;
				}
				break;
		}

		/* Slide window (if no override was activated) */
		if(!overrideSlide) {
			MI0 = MI1;
			MI1 = MI2;
			MI2 = MI3;
			if(MI == MBB.end())
				return rv;
			MI3 = MI++;

			point++;
		}
		else {
			overrideSlide = false;
		}
	}

	return rv;
}

/**
 * @brief Expansion macro: Insert 3 NOPs.
 */
#define EXPAND_3NOP() {\
	MI = BuildMI(*MBB, MI, DL, Subtarget->getInstrInfo()->get(RISCV::ADDI), rvRegs[0])\
			.addReg(rvRegs[0]).addImm(0);\
	MI = BuildMI(*MBB, MI, DL, Subtarget->getInstrInfo()->get(RISCV::ADDI), rvRegs[0])\
			.addReg(rvRegs[0]).addImm(0);\
	MI = BuildMI(*MBB, MI, DL, Subtarget->getInstrInfo()->get(RISCV::ADDI), rvRegs[0])\
			.addReg(rvRegs[0]).addImm(0);\
}

/**
 * @brief Expansion macro: Insert an immediate vector arithmetic operation.
 */
#define EXPAND_OPIV(op, rc, ra, ib) {\
	MI = BuildMI(*MBB, MI, DL, Subtarget->getInstrInfo()->get(op), rc)\
			.addReg(ra).addImm(ib);\
}

/**
 * @brief Expansion macro: Insert a word store.
 */
#define EXPAND_SW(ra, off, rc) {\
	MI = BuildMI(*MBB, MI, DL, Subtarget->getInstrInfo()->get(RISCV::SW), ra)\
			.addImm(off).addReg(rc);\
}

/**
 * @brief Expansion macro: Insert a vector arithmetic operation.
 */
#define EXPAND_OPV(op, rc, ra, rb) {\
	MI = BuildMI(*MBB, MI, DL, Subtarget->getInstrInfo()->get(op), rc)\
			.addReg(ra).addReg(rb);\
}

/**
 * @brief Expansion macro: Insert a word load.
 */
#define EXPAND_LW(rc, off, ra) {\
	MI = BuildMI(*MBB, MI, DL, Subtarget->getInstrInfo()->get(RISCV::LW), rc)\
			.addImm(off).addReg(ra);\
}

/**
 * @brief Expansion macro: Insert an arithmetic operation.
 */
#define EXPAND_OP(op, rc, ra, rb) {\
	MI = BuildMI(*MBB, MI, DL, Subtarget->getInstrInfo()->get(op), rc)\
			.addReg(ra).addReg(rb);\
}

/**
 * @brief Expansion macro: Insert an immediate load.
 */
#define EXPAND_LI(rc, ia) {\
	MI = BuildMI(*MBB, MI, DL, Subtarget->getInstrInfo()->get(RISCV::LI), rc)\
			.addImm(ia);\
}

/**
 * @brief Substitute all present matches in the matches vectors.
 */
void RISCVVectorInstrBuilder::substituteAllMatches(MachineBasicBlock *MBB, const RISCVSubtarget *Subtarget) {
	/**
	 * The beginning of a block is stored in the matches list as the position of first instruction.
	 * This means that substituting from top to bottom of a basic block would invalidate such data
	 * (since the number of instructions will vary from before to after). Thus, we iterate the list
	 * in reverse, substituting from bottom to top.
	 */
	for(int i = (getListSize() - 1); i >= 0; i--) {
		unsigned int j = 0;
		bool isInside = false;
		DebugLoc DL;

		/* First step: Remove matched instructions */
		// TODO: Implement some sort of lower limitant: If the block size is too small, there is no reason to perform substitution.
		for(MachineBasicBlock::iterator MI = MBB->begin(); MI != MBB->end(); j++) {
			/* Each case must be dealt separately, since block sizes varies */
			switch(getClassAt(i)) {
				case RR:
					isInside = (j >= getStartPointAt(i) && j < (getStartPointAt(i) + (getBlockSizeAt(i) * 4)));
					break;
				case RI:
					isInside = (j >= getStartPointAt(i) && j < (getStartPointAt(i) + (getBlockSizeAt(i) * 3)));
					break;
				case IR:
					isInside = (j >= getStartPointAt(i) && j < (getStartPointAt(i) + ((getBlockSizeAt(i) - 1) * 3) + 4));
					break;
			}

			/* We're inside the block. Remove instructions */
			if(isInside) {
				// TODO: Remove
				std::cout << "Removing " << MI->getOpcode() << " at position " << j << " with DebugLoc " << MI->getDebugLoc() << std::endl;
				DL = MI->getDebugLoc();
				MI = MBB->erase(MI);
			}
			else {
				++MI;
			}
		}

		/* Second step: Add new instructions */
		j = 0;
		for(MachineBasicBlock::iterator MI = MBB->begin(); MI != MBB->end(); j++) {
			if(getStartPointAt(i) == j) {
				/* Calculate how many unrolls will be performed, since only XVEC_AVAIL_REGS are available at a time */
				unsigned int opsAmt = std::ceil(getBlockSizeAt(i) / (double) XVEC_AVAIL_REGS);
				unsigned int opsRem = getBlockSizeAt(i) % XVEC_AVAIL_REGS;
				unsigned int opsCur;

				/* opsRem == 0 means that a whole full of XVEC_AVAIL_REGS registers should be operated */
				if(!opsRem)
					opsRem = XVEC_AVAIL_REGS;

				/**
				 * Note: all instructions are added in reverse order!
				 */
				switch(getClassAt(i)) {
					case RR:
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						/* ADDIV: Restore general purpose registers */
						EXPAND_OPIV(RISCV::ADDIV, rvRegs[1], rvRegs[3], 0);
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						/* Iterate every XVEC_AVAIL_REGS */
						for(int k = opsAmt - 1; k >= 0; k--) {
							opsCur = ((opsAmt - 1) == (unsigned int) k)? opsRem : XVEC_AVAIL_REGS;

							/* Store results */
							for(int l = opsCur - 1; l >= 0; l--)
								EXPAND_SW(rvRegs[l + 1], (4 * ((XVEC_AVAIL_REGS * k) + l)), rvRegs[31]);
							/* Isolation of vector operation with 3 NOPs */
							EXPAND_3NOP();
							/* Vector operation: c[] = a[] OP b[] */
							EXPAND_OPV(getEqVectorOpcodeAt(i), rvRegs[1], rvRegs[2], rvRegs[1]);
							/* Isolation of vector operation with 3 NOPs */
							EXPAND_3NOP();
							/* Load b[] operands */
							for(int l = opsCur - 1; l >= 0; l--)
								EXPAND_LW(rvRegs[l + 1], (4 * ((XVEC_AVAIL_REGS * k) + l)), rvRegs[30]);
							/* Isolation of vector operation with 3 NOPs */
							EXPAND_3NOP();
							/* ADDIV: Move a[] operands */
							EXPAND_OPIV(RISCV::ADDIV, rvRegs[2], rvRegs[1], 0);
							/* Isolation of vector operation with 3 NOPs */
							EXPAND_3NOP();
							/* Load a[] operands */
							for(int l = opsCur - 1; l >= 0; l--)
								EXPAND_LW(rvRegs[l + 1], (4 * ((XVEC_AVAIL_REGS * k) + l)), rvRegs[29]);
						}
						/* Move c[] indexer to register 31 */
						EXPAND_OP(RISCV::ADD, rvRegs[31], getXCIdxAt(i), rvRegs[0]);
						/* Move b[] indexer to register 30 */
						EXPAND_OP(RISCV::ADD, rvRegs[30], getXBIdxAt(i), rvRegs[0]);
						/* Move a[] indexer to register 29 */
						EXPAND_OP(RISCV::ADD, rvRegs[29], getXAIdxAt(i), rvRegs[0]);
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						/* ADDIV: Save general purpose registers */
						EXPAND_OPIV(RISCV::ADDIV, rvRegs[3], rvRegs[1], 0);
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						break;
					case RI:
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						/* ADDIV: Restore general purpose registers */
						EXPAND_OPIV(RISCV::ADDIV, rvRegs[1], rvRegs[3], 0);
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						/* Iterate every XVEC_AVAIL_REGS */
						for(int k = opsAmt - 1; k >= 0; k--) {
							opsCur = ((opsAmt - 1) == (unsigned int) k)? opsRem : XVEC_AVAIL_REGS;

							/* Store results */
							for(int l = opsCur - 1; l >= 0; l--)
								EXPAND_SW(rvRegs[l + 1], (4 * ((XVEC_AVAIL_REGS * k) + l)), rvRegs[31]);
							/* Isolation of vector operation with 3 NOPs */
							EXPAND_3NOP();
							/* Vector operation: c[] = a[] OPI b */
							EXPAND_OPIV(getEqVectorOpcodeAt(i), rvRegs[1], rvRegs[1], getXImmAt(i));
							/* Isolation of vector operation with 3 NOPs */
							EXPAND_3NOP();
							/* Load a[] operands */
							for(int l = opsCur - 1; l >= 0; l--)
								EXPAND_LW(rvRegs[l + 1], (4 * ((XVEC_AVAIL_REGS * k) + l)), rvRegs[29]);
						}
						/* Move c[] indexer to register 31 */
						EXPAND_OP(RISCV::ADD, rvRegs[31], getXCIdxAt(i), rvRegs[0]);
						/* Move a[] indexer to register 29 */
						EXPAND_OP(RISCV::ADD, rvRegs[29], getXAIdxAt(i), rvRegs[0]);
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						/* ADDIV: Save general purpose registers */
						EXPAND_OPIV(RISCV::ADDIV, rvRegs[3], rvRegs[1], 0);
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						break;
					case IR:
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						/* ADDIV: Restore general purpose registers */
						EXPAND_OPIV(RISCV::ADDIV, rvRegs[1], rvRegs[3], 0);
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						/* Iterate every XVEC_AVAIL_REGS */
						for(int k = opsAmt - 1; k >= 0; k--) {
							opsCur = ((opsAmt - 1) == (unsigned int) k)? opsRem : XVEC_AVAIL_REGS;

							/* Store results */
							for(int l = opsCur - 1; l >= 0; l--)
								EXPAND_SW(rvRegs[l + 1], (4 * ((XVEC_AVAIL_REGS * k) + l)), rvRegs[31]);
							/* Isolation of vector operation with 3 NOPs */
							EXPAND_3NOP();
							/* Vector operation: c[] = a OP b[] */
							EXPAND_OPV(getEqVectorOpcodeAt(i), rvRegs[1], rvRegs[2], rvRegs[1]);
							/* Isolation of vector operation with 3 NOPs */
							EXPAND_3NOP();
							/* Load b[] operands */
							for(int l = opsCur - 1; l >= 0; l--)
								EXPAND_LW(rvRegs[l + 1], (4 * ((XVEC_AVAIL_REGS * k) + l)), rvRegs[30]);
						}
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						/* ADDIV: Move a[] operands */
						EXPAND_OPIV(RISCV::ADDIV, rvRegs[2], rvRegs[1], 0);
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						/* Load a[] operands */
						for(int l = XVEC_AVAIL_REGS; l > 0; l--)
							EXPAND_LI(rvRegs[l], getXImmAt(i));
						/* Move c[] indexer to register 31 */
						EXPAND_OP(RISCV::ADD, rvRegs[31], getXCIdxAt(i), rvRegs[0]);
						/* Move b[] indexer to register 30 */
						EXPAND_OP(RISCV::ADD, rvRegs[30], getXBIdxAt(i), rvRegs[0]);
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						/* ADDIV: Save general purpose registers */
						EXPAND_OPIV(RISCV::ADDIV, rvRegs[3], rvRegs[1], 0);
						/* Isolation of vector operation with 3 NOPs */
						EXPAND_3NOP();
						break;
				}
				break;
			}
			else {
				++MI;
			}
		}
	}
}
