/*
	Copyright (c) 2017-2018, Zhengyang Gu (author)
 
	All rights reserved.

	Redistribution and use in source and binary forms, with or without 
	modification, are permitted provided that the following conditions
	are met:

	*	Redistributions of source code must retain the above
		copyright notice, this list of conditions and the following
		disclaimer.

	*	Redistributions in binary form must reproduce the above
		copyright notice, this list of conditions and the following
		disclaimer in the documentation and/or other materials
		provided with the distribution.

	*	Neither the name of the author nor the names of its
		contributors may be used to endorse or promote products
		derived from this software without specific prior written 
		permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
	COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
	POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <string.h>
#include "sf.h"
#include "instr-riscv.h"
#include "mextern.h"

void
print_bin_instr(Engine* E, State* S, int32_t m, int format_op)/* Will delete later...*/
{ 
	uint32_t binaryNum[32];
	uint32_t n = (uint32_t) m;
	int i;
	for (i = 31; i>=0; i--)
	{ 
		binaryNum[i] = n % 2; 
		n = n / 2;
	}
	for (i = 0; i<32; i++)
	{
		mprint(E, S, nodeinfo, "%d", binaryNum[i]);
		if (format_op)
		{
			if (i == 6 ||i == 11 || i== 16 || i==19 || i==24)
			{
				mprint(E, S, nodeinfo, " ");
			}
		}
		else
		{
			if (i == 3 ||i == 7 || i== 11 || i==15 || i==19 || i==23 || i==27)
			{
				mprint(E, S, nodeinfo, " ");
			}
		}
	}
}


/*	Arithmetic instr are forwarded, so dependent arithmetic instr do not stall	*/
/*	LOAD instr are forwarded, so cause only 1 stall					*/
/*	BRANCH instr are tested in an earlier stage to reduce cost of incorrect branch	*/
/*	JUMP locations are calculated in an ID/EX stage so cost 1/2 cycles		*/
/*	BRANCH instr always take the wrong path, so always stall 1 cycle		*/



//Hazards that cause stalling:
//	1 stall:
//		LOAD instructions that write to a register-required-by-the-next-instruction after reading from memory:
//			...EX of next instruction needs reg data from MA of LOAD instructions.
//		JAL instruction calculates the PC of next instruction using arithmetic in ID.
//			...IF of next instruction needs PC from ID of JAL instruction.
//		BRANCH instructions test for (in)equality in ID, dependent on previous instruction:
//			...ID of BRANCH istruction needs reg data from EX of previous instruction.
//		BRANCH instruction guessed incorrectly, so need to flush:
//			...IF of next instruction needs PC from ID of BRANCH instruction.
//	2 stalls:
//		LOAD instruction followed by dependent BRANCH instruction:
//			...ID of BRANCH instruction needs reg data from MA of LOAD instructions.
//		JALR instruction calculates the PC of next instruction by using register.
//			...IF of next instruction needs reg data from EX of JALR instruction.

int
riscvbranches(int op)
{
	switch(op)
	{
		case RISCV_OP_BEQ:
		case RISCV_OP_BNE:
		case RISCV_OP_BLT:
		case RISCV_OP_BGE:
		case RISCV_OP_BLTU:
		case RISCV_OP_BGEU:
		{
			return 1;
		}
	}
	return 0;
}

int
riscvloads(int op)
{
	switch (op)
	{
		case RISCV_OP_LB:
		case RISCV_OP_LH:
		case RISCV_OP_LW:
		case RISCV_OP_LBU:
		case RISCV_OP_LHU:

		case RV32F_OP_FLW:
		case RV32D_OP_FLD:
		{
			return 1;
		}
	}

	return 0;
}

int
riscvreadsreg(int op)
{
	switch(op)
	{
		case RISCV_OP_JALR:
		case RISCV_OP_LB:
		case RISCV_OP_LH:
		case RISCV_OP_LW:
		case RISCV_OP_LBU:
		case RISCV_OP_LHU:
		case RISCV_OP_ADDI:
		case RISCV_OP_SLTI:
		case RISCV_OP_SLTIU:
		case RISCV_OP_XORI:
		case RISCV_OP_ORI:
		case RISCV_OP_ANDI:
		case RISCV_OP_SLLI:
		case RISCV_OP_SRLI:
		case RISCV_OP_SRAI:
		{
			return 1;
		}
		case RISCV_OP_BEQ:
		case RISCV_OP_BNE:
		case RISCV_OP_BLT:
		case RISCV_OP_BGE:
		case RISCV_OP_BLTU:
		case RISCV_OP_BGEU:
		case RISCV_OP_SB:
		case RISCV_OP_SH:
		case RISCV_OP_SW:
		case RISCV_OP_ADD:
		case RISCV_OP_SUB:
		case RISCV_OP_SLL:
		case RISCV_OP_SLT:
		case RISCV_OP_SLTU:
		case RISCV_OP_XOR:
		case RISCV_OP_SRL:
		case RISCV_OP_SRA:
		case RISCV_OP_OR:
		case RISCV_OP_AND:
		{
			return 2;
		}
	}
	return 0;
}

int
riscvsetsreg(int op)
{
	switch(op)
	{
		/*	LUI and AUIPC: Info is there by end of ID so no need to stall for next instr	*/
		/*	JAL and JALR cause their own stall(s) anyway	*/
		/*	LOADs have their own stall tests	*/
		case RISCV_OP_ADDI:
		case RISCV_OP_SLTI:
		case RISCV_OP_SLTIU:
		case RISCV_OP_XORI:
		case RISCV_OP_ORI:
		case RISCV_OP_ANDI:
		case RISCV_OP_SLLI:
		case RISCV_OP_SRLI:
		case RISCV_OP_SRAI:
		case RISCV_OP_ADD:
		case RISCV_OP_SUB:
		case RISCV_OP_SLL:
		case RISCV_OP_SLT:
		case RISCV_OP_SLTU:
		case RISCV_OP_XOR:
		case RISCV_OP_SRL:
		case RISCV_OP_SRA:
		case RISCV_OP_OR:
		case RISCV_OP_AND:
		{
			return 1;
		}
	}
	return 0;
}

int
riscvnumstalls(RiscvPipestage IDstage, RiscvPipestage IFstage)
{
	uint8_t IDrd	= (IDstage.instr&Bits7to11) >> 7;
	uint8_t IFrs1	= (IFstage.instr&Bits15to19) >> 15;
	uint8_t IFrs2	= (IFstage.instr&Bits20to24) >> 20;

	if (riscvloads(IDstage.op))
	{
		if (riscvbranches(IFstage.op))
		{
			if (IFrs1 == IDrd || IFrs2 == IDrd)
			{
				return 2;
			}
		}
		else if (riscvreadsreg(IFstage.op) == 1)
		{
			if (IFrs1 == IDrd)
			{
				return 1;
			}
		}
		else if (riscvreadsreg(IFstage.op) == 2)
		{
			if (IFrs1 == IDrd || IFrs2 == IDrd)
			{
				return 1;
			}
		}
	}
	if (riscvsetsreg(IDstage.op))
	{
		if (riscvbranches(IFstage.op))
		{
			if (IFrs1 == IDrd || IFrs2 == IDrd)
			{
				return 1;
			}
		}
	}

	return 0;
}

int
riscvfaststep(Engine *E, State *S, int drain_pipeline)
{
	int		i;
	uint32_t	tmpinstr;
	uint32_t	tmpPC;
	Picosec		saved_globaltime;


	USED(drain_pipeline);

	saved_globaltime = E->globaltimepsec;
	for (i = 0; (i < E->quantum) && E->on && S->runnable; i++)
	{
		if (!eventready(E->globaltimepsec, S->TIME, S->CYCLETIME))
		{
			E->globaltimepsec = max(E->globaltimepsec, S->TIME) + S->CYCLETIME;
			continue;
		}
		/*	the superH equivalent checks for exceptions/interrupts,	*/
		/*	and whether it is in power-down (sleep) mode		*/

		tmpPC = S->PC;
		tmpinstr = superHreadlong(E, S, S->PC);

		riscvdecode(E, tmpinstr, &(S->riscv->P.EX));

		S->riscv->instruction_distribution[S->riscv->P.EX.op]++;

		S->riscv->P.EX.fetchedpc = S->PC;
		S->PC += 4;
		S->CLK++;
		S->ICLK++;
		S->dyncnt++;
		S->TIME += S->CYCLETIME;

		switch (S->riscv->P.EX.format)
		{
			case INSTR_R:
			{
				instr_r *tmp;

				tmp = (instr_r *)&S->riscv->P.EX.instr;
				(*(S->riscv->P.EX.fptr))(E, S, tmp->rs1, tmp->rs2, tmp->rd);
				break;
			}

			case INSTR_I:
			{
				instr_i *tmp;

				tmp = (instr_i *)&S->riscv->P.EX.instr;
				(*(S->riscv->P.EX.fptr))(E, S, tmp->rs1, tmp->rd, tmp->imm0);
				break;
			}

			case INSTR_S:
			{
				instr_s *tmp;

				tmp = (instr_s *)&S->riscv->P.EX.instr;
				(*(S->riscv->P.EX.fptr))(E, S, tmp->rs1, tmp->rs2, tmp->imm0, tmp->imm5);
				break;
			}

			case INSTR_B:
			{
				instr_b *tmp;

				tmp = (instr_b *)&S->riscv->P.EX.instr;
				(*(S->riscv->P.EX.fptr))(E, S, tmp->rs1, tmp->rs2, tmp->imm1, tmp->imm5, tmp->imm11, tmp->imm12);
				break;
			}

			case INSTR_U:
			{
				instr_u *tmp;

				tmp = (instr_u *)&S->riscv->P.EX.instr;
				(*(S->riscv->P.EX.fptr))(E, S, tmp->rd, tmp->imm0);
				break;
			}

			case INSTR_J:
			{
				instr_j *tmp;

				tmp = (instr_j *)&S->riscv->P.EX.instr;
				(*(S->riscv->P.EX.fptr))(E, S, tmp->rd, tmp->imm1, tmp->imm11, tmp->imm12, tmp->imm20);
				break;
			}
			
			case INSTR_R4:
			{
				instr_r4 *tmp;

				tmp = (instr_r4 *)&S->riscv->P.EX.instr;
				(*(S->riscv->P.EX.fptr))(E, S, tmp->rs1, tmp->rs2, tmp->rs3, tmp->rm, tmp->rd);
				break;
			}

			case INSTR_N:
			{
				(*(S->riscv->P.EX.fptr))(E, S);
				break;
			}

			default:
			{
				sfatal(E, S, "Unknown Instruction Type !!");
				break;
			}
		}

		if (SF_BITFLIP_ANALYSIS)
		{
			S->Cycletrans += bit_flips_32(tmpPC, S->PC);	
			S->Cycletrans = 0;
		}
/*	Not needed for fast step...?
		if (S->pipeshow)
		{					
			riscvdumppipe(E, S);
		}
*/
		E->globaltimepsec = max(E->globaltimepsec, S->TIME) + S->CYCLETIME;
	}
	E->globaltimepsec = saved_globaltime;
	S->last_stepclks = i;

	return i;
}

int
riscvstep(Engine *E, State *S, int drain_pipeline)
{
	int		i, exec_energy_updated = 0, stall_energy_updated = 0;
	ulong		tmpPC;
	Picosec		saved_globaltime;
	S->superH->SR.MD = 1;

	saved_globaltime = E->globaltimepsec;
	for (i = 0; (i < E->quantum) && E->on && S->runnable; i++)
	{
		/*	superH equivalent has some sort of bus locking managment inserted here.	*/

		if (!drain_pipeline)
		{
			if (!eventready(E->globaltimepsec, S->TIME, S->CYCLETIME))
			{
				E->globaltimepsec = max(E->globaltimepsec, S->TIME) + S->CYCLETIME;
				continue;
			}

			/*	superH equivalent has interrupt and exception managment inserted here.	*/

		}

		tmpPC = S->PC;

		/*								*/
		/* 	 		Clear WB stage				*/
		/*								*/
		S->riscv->P.WB.valid = 0;


		/*								*/
		/*   MA cycles--. If 0, move instr in MA to WB if WB is empty	*/
		/*								*/
		if ((S->riscv->P.MA.valid) && (S->riscv->P.MA.cycles > 0))
		{
			S->riscv->P.MA.cycles -= 1;

			/*							*/
			/*	For mem stall, energy cost assigned is NOP	*/
			/*							*/

			if (SF_POWER_ANALYSIS)
			{
				update_energy(SUPERH_OP_NOP, 0, 0);
				stall_energy_updated = 1;
			}

		}

		if ((S->riscv->P.MA.valid) && (S->riscv->P.MA.cycles == 0)
			&& (!S->riscv->P.WB.valid))
		{
			/*		Count # bits flipping in WB		*/
			if (SF_BITFLIP_ANALYSIS)
			{
				S->Cycletrans += bit_flips_32(S->riscv->P.MA.instr,
							S->riscv->P.WB.instr);
			}

			memmove(&S->riscv->P.WB, &S->riscv->P.MA, sizeof(RiscvPipestage));
			S->riscv->P.MA.valid = 0;
			S->riscv->P.WB.valid = 1;
		}


		/*										*/
		/* 	 EX cycles--. If 0, exec, mark EX stage empty and move it to MA		*/
		/*										*/
		if ((S->riscv->P.EX.valid) && (S->riscv->P.EX.cycles > 0))
		{
			S->riscv->P.EX.cycles -= 1;
/*	Power analysis for riscv not implemented...?
			if (SF_POWER_ANALYSIS)
			{
				update_energy(S->riscv->P.EX.op, 0, 0);
				exec_energy_updated = 1;
			}
*/
		}

		if (S->riscv->P.EX.valid && (S->riscv->P.EX.fptr == NULL))
		{
			mprint(E, S, nodeinfo, "PC=0x" UHLONGFMT "\n",
				S->riscv->P.EX.fetchedpc);
			mprint(E, S, nodeinfo, "S->riscv->P.EX.instr = [0x%x]",
				S->riscv->P.EX.instr);
			sfatal(E, S, "Illegal instruction.");
		}

		if (	(S->riscv->P.EX.valid)
			&& (S->riscv->P.EX.cycles == 0)
			&& !(S->riscv->P.MA.valid)
		)
		{

			/*	Prevents next 2 instructions from moving to EX if jumping	*/
			if (S->riscv->P.EX.op == RISCV_OP_JALR || riscvbranches(S->riscv->P.EX.op))
			{
				/*	set PC back so that JALR and BRANCH instr can work	*/
				if (SF_BITFLIP_ANALYSIS)
				{
					S->Cycletrans += bit_flips_32(S->riscv->P.ID.fetchedpc + 4, S->PC);
				}
				S->PC = S->riscv->P.EX.fetchedpc + 4;

				S->riscv->P.ID.valid = 0;/*	Same effect as flushing	*/
				S->riscv->P.IF.valid = 0;
			}

			switch (S->riscv->P.EX.format)
			{
				case INSTR_N:
				{
					(*(S->riscv->P.EX.fptr))(E, S);	/*	riscv_nop?	*/
					S->dyncnt++;

					break;
				}

				case INSTR_R:
				{
					uint32_t tmp = (uint32_t) S->riscv->P.EX.instr;
					(*(S->riscv->P.EX.fptr))(E, S,
								(tmp&Bits15to19) >> 15,
								(tmp&Bits20to24) >> 20,
								(tmp&Bits7to11) >> 7);
					S->dyncnt++;

					break;
				}

				case INSTR_I:
				{
					uint32_t tmp = (uint32_t) S->riscv->P.EX.instr;
					(*(S->riscv->P.EX.fptr))(E, S,
								(tmp&Bits15to19) >> 15,
								(tmp&Bits7to11) >> 7,
								(tmp&Bits20to31) >> 20);
					S->dyncnt++;

					break;
				}

				case INSTR_S:
				{
					uint32_t tmp = (uint32_t) S->riscv->P.EX.instr;
					(*(S->riscv->P.EX.fptr))(E, S,
								(tmp&Bits15to19) >> 15,
								(tmp&Bits20to24) >> 20,
								(tmp&Bits7to11) >> 7,
								(tmp&Bits25to31) >> 25);
					S->dyncnt++;

					break;
				}

				case INSTR_B:
				{
					uint32_t tmp = (uint32_t) S->riscv->P.EX.instr;
					(*(S->riscv->P.EX.fptr))(E, S,
								(tmp&Bits15to19) >> 15,
								(tmp&Bits20to24) >> 20,
								(tmp&Bits8to11) >> 8,
								(tmp&Bits25to30) >> 25,
								(tmp&Bit7) >> 7,
								(tmp&Bit31) >> 31);
					S->dyncnt++;

					break;
				}

				case INSTR_U:
				{
					uint32_t tmp = (uint32_t) S->riscv->P.EX.instr;
					(*(S->riscv->P.EX.fptr))(E, S,
								(tmp&Bits7to11) >> 7,
								(tmp&Bits12to31) >> 12);
					S->dyncnt++;

					break;
				}

				case INSTR_J:
/*	There is only one instruction of J-type, which is JAL, which does early PC calculation	*/
				{
/*	Do nothing, because its already done in ID stage
					uint32_t tmp = S->riscv->P.EX.instr;
					(*(S->riscv->P.EX.fptr))(E, S,
								(tmp&Bits7to11) >> 7,
								(tmp&Bits21to30) >> 21,
								(tmp&Bit20) >> 20,
								(tmp&Bits12to19) >> 12,
								(tmp&Bit31) >> 31);
					S->dyncnt++;
*/
					break;
				}

				default:
				{
					sfatal(E, S, "Unknown Instruction Type !!");
					break;
				}
			}

			/*		Count # bits flipping in MA		*/
			if (SF_BITFLIP_ANALYSIS)
			{
				S->Cycletrans += bit_flips_32(S->riscv->P.EX.instr,
							S->riscv->P.MA.instr);
			}


			memmove(&S->riscv->P.MA, &S->riscv->P.EX, sizeof(RiscvPipestage));
			S->riscv->P.EX.valid = 0;
			S->riscv->P.MA.valid = 1;
		}

		/*	     First : If fetch unit is stalled, dec its counter		*/
		if (S->riscv->P.fetch_stall_cycles > 0)
		{
			/*								*/
			/*	Fetch Unit is stalled. Decrement time for it to wait.	*/
			/*	If we have not accounted for energy cost of stall 	*/
			/*	above (i.e. no stalled instr in MA), then cost of this	*/
			/*	cycle is calculated as cost of a NOP.			*/
			/*								*/
			S->riscv->P.fetch_stall_cycles--;

			if (SF_POWER_ANALYSIS)
			{
				if (!stall_energy_updated && !exec_energy_updated)
				{
					update_energy(SUPERH_OP_NOP, 0, 0);
				}
			}
		}
		/*									*/
		/* 	move instr in ID stage to EX stage if EX stage is empty.	*/
		/*									*/
		if (	(S->riscv->P.ID.valid)
			&& (S->riscv->P.fetch_stall_cycles == 0)
			&& (!S->riscv->P.EX.valid)
		)
		{
			/*		Count # bits flipping in EX		*/
			if (SF_BITFLIP_ANALYSIS)
			{
				S->Cycletrans += bit_flips_32(S->riscv->P.ID.instr,
							S->riscv->P.EX.instr);
			}
			/* I put this in IF stage so that dumppipe provides info on IF/ID stages too	*/
			//riscvdecode(E, S->riscv->P.ID.instr, &S->riscv->P.ID);

			/*	Now we should have the PC and imm data needed to calculate branch	*/
			/*	targets, unless there is dependancy on the previous instruction.	*/


			/*	check if need to stall next instuction (currently in IF)	*/
			S->riscv->P.fetch_stall_cycles += riscvnumstalls(S->riscv->P.ID, S->riscv->P.IF);

			/*	Stops next instr from moving to ID if jump/branch. Assumes next instr is always wrong.	*/
			if (S->riscv->P.ID.op == RISCV_OP_JAL)
			{
				if (SF_BITFLIP_ANALYSIS)
				{
					S->Cycletrans += bit_flips_32(S->riscv->P.ID.fetchedpc + 4, S->PC);
				}
				S->PC = S->riscv->P.ID.fetchedpc + 4;/*	set PC back to when it was at JAL instr	*/

				uint32_t tmp = S->riscv->P.ID.instr;
				(*(S->riscv->P.ID.fptr))(E, S,
							(tmp&Bits7to11) >> 7,
							(tmp&Bits21to30) >> 21,
							(tmp&Bit20) >> 20,
							(tmp&Bits12to19) >> 12,
							(tmp&Bit31) >> 31);
				S->dyncnt++;

				S->riscv->P.IF.valid = 0;/*	Same effect as flushing	*/
			}

			memmove(&S->riscv->P.EX, &S->riscv->P.ID, sizeof(RiscvPipestage));

			S->riscv->P.ID.valid = 0;
			S->riscv->P.EX.valid = 1;
		}

		/*									*/
		/* 	    Move instr in IF stage to ID stage if ID stage is empty	*/
		/*									*/
		if (	!(S->riscv->P.ID.valid)
			&& (S->riscv->P.IF.valid)
			&& (S->riscv->P.fetch_stall_cycles == 0))
		{
			/*		Count # bits flipping in ID		*/
			if (SF_BITFLIP_ANALYSIS)
			{
				S->Cycletrans += bit_flips_32(S->riscv->P.ID.instr,
						S->riscv->P.IF.instr);
			}

			memmove(&S->riscv->P.ID, &S->riscv->P.IF, sizeof(RiscvPipestage));
			S->riscv->P.IF.valid = 0;
			S->riscv->P.ID.valid = 1;

		}

		/*									*/
		/* 	  Put instr in IF stage if it is empty, and increment PC	*/
		/*									*/
		if (	!(S->riscv->P.IF.valid)
		)
		{
			uint32_t	instrlong;

			/*						*/
			/*	Get inst from mem hierarchy or fetch	*/
			/*	NOPs (used for draining pipeline).	*/
			/*						*/
			if (drain_pipeline)
			{
				instrlong = 51;/*	should be 0x00000000000000000000000000110011 for ADD x0,x0,x0?	*/
			}
			else
			{
/*	Riscv Cache not implemented...
				S->riscv->mem_access_type = MEM_ACCESS_IFETCH;
				instrlong = superHreadword(E, S, S->PC);
				S->nfetched++;
				S->riscv->mem_access_type = MEM_ACCESS_NIL;
*/
				instrlong = superHreadlong(E, S, S->PC);
				S->nfetched++;
			}

			/*   Count # bits flipping in IF		*/
			if (SF_BITFLIP_ANALYSIS)
			{
				S->Cycletrans += bit_flips_32(S->riscv->P.IF.instr, instrlong);
			}

			S->riscv->P.IF.instr = instrlong;
			S->riscv->P.IF.valid = 1;

			/*						*/
			/*	We also set this here (early) to	*/
			/*	enable intr/excp handling, since	*/
			/*	there, we do not drain the pipeline	*/
			/*	if instr in IF is of type which uses	*/
			/*	delay slot.				*/
			/*						*/
			riscvdecode(E, S->riscv->P.IF.instr, &S->riscv->P.IF);

			if (!drain_pipeline)
			{
				S->riscv->P.IF.fetchedpc = S->PC;
				S->PC += 4;
			}
		}

		S->CLK++;
		S->ICLK++;
		S->TIME += S->CYCLETIME;

		if (S->pipeshow)
		{
			riscvdumppipe(E, S);
		}

/*	Power Adaptation Unit not implemented...
		if (SF_PAU_DEFINED && S->riscv->PAUs != NULL)
		{
			pau_clk(E, S);
		}
*/
		if (SF_BITFLIP_ANALYSIS)
		{
			S->Cycletrans += bit_flips_32(tmpPC, S->PC);
			S->energyinfo.ntrans = S->energyinfo.ntrans + S->Cycletrans;
			S->Cycletrans = 0;
		}

		E->globaltimepsec = max(E->globaltimepsec, S->TIME) + S->CYCLETIME;
	}
	E->globaltimepsec = saved_globaltime;

	return i;
}

void
riscvdumppipe(Engine *E, State *S)
{
	mprint(E, S, nodeinfo, "\nnode ID=%d, PC=0x" UHLONGFMT ", ICLK=" UVLONGFMT ". ",
			S->NODE_ID, S->PC, S->ICLK);
	if (S->riscv->P.WB.valid)
	{
		mprint(E, S, nodeinfo, "WB: [%s]\tinstr: [", riscv_opstrs[S->riscv->P.WB.op]);
	print_bin_instr(E,S,S->riscv->P.WB.instr,1);
	mprint(E, S, nodeinfo, "]\tfetched: [0x%x]\n",S->riscv->P.WB.fetchedpc);
	}
	else
	{
		mprint(E, S, nodeinfo, "WB: []\n");
	}

	if (S->riscv->P.MA.valid)
	{
		mprint(E, S, nodeinfo, "MA: [%s]\tinstr: [", riscv_opstrs[S->riscv->P.MA.op]);
	print_bin_instr(E,S,S->riscv->P.MA.instr,1);
	mprint(E, S, nodeinfo, "]\tfetched: [0x%x]\n",S->riscv->P.MA.fetchedpc);
	}
	else
	{
		mprint(E, S, nodeinfo, "MA: []\n");
	}

	if (S->riscv->P.EX.valid)
	{
		mprint(E, S, nodeinfo, "EX: [%s]\tinstr: [", riscv_opstrs[S->riscv->P.EX.op]);
		print_bin_instr(E,S,S->riscv->P.EX.instr,1);
		mprint(E, S, nodeinfo, "]\tfetched: [0x%x]\n",S->riscv->P.EX.fetchedpc);
	}
	else
	{
		mprint(E, S, nodeinfo, "EX: []\n");
	}

	if (S->riscv->P.ID.valid)
	{
		mprint(E, S, nodeinfo, "ID: [%s]\tinstr: [", riscv_opstrs[S->riscv->P.ID.op]);
		print_bin_instr(E,S,S->riscv->P.ID.instr,1);
		mprint(E, S, nodeinfo, "]\tfetched: [0x%x]\n",S->riscv->P.ID.fetchedpc);
	}
	else
	{
		mprint(E, S, nodeinfo, "ID: []\n");
	}

	if (S->riscv->P.IF.valid)
	{
		mprint(E, S, nodeinfo, "IF: [%s]\tinstr: [", riscv_opstrs[S->riscv->P.IF.op]);
		print_bin_instr(E,S,S->riscv->P.IF.instr,1);
		mprint(E, S, nodeinfo, "]\tfetched: [0x%x]\n\n",S->riscv->P.IF.fetchedpc);
	}
	else
	{
		mprint(E, S, nodeinfo, "IF: []\n\n");
	}

	return;
}

void
riscvdumpdistribution(Engine *E, State *S)
{
	for(int i = 0; i < RISCV_OP_MAX; i++) {
		mprint(E, S, nodeinfo, "%-8s {%d}\n", riscv_opstrs[i], S->riscv->instruction_distribution[i]);
	}

	return;
}

void
riscvpipeflush(State *S)
{
	/*								*/
	/*	Flush pipeline, count # bits we clear in pipe regs	*/
	/*								*/
	S->riscv->P.IF.cycles = 0;
	S->riscv->P.IF.valid = 0;

	S->riscv->P.ID.cycles = 0;
	S->riscv->P.ID.valid = 0;

	S->riscv->P.EX.cycles = 0;
	S->riscv->P.EX.valid = 0;

	S->riscv->P.MA.cycles = 0;
	S->riscv->P.MA.valid = 0;

	S->riscv->P.WB.cycles = 0;
	S->riscv->P.WB.valid = 0;

	if (SF_BITFLIP_ANALYSIS)
	{
		S->Cycletrans += bit_flips_32(S->riscv->P.IF.instr, 0);
		S->Cycletrans += bit_flips_32(S->riscv->P.ID.instr, 0);
		S->Cycletrans += bit_flips_32(S->riscv->P.EX.instr, 0);
		S->Cycletrans += bit_flips_32(S->riscv->P.MA.instr, 0);
		S->Cycletrans += bit_flips_32(S->riscv->P.WB.instr, 0);
	}

	return;
}

