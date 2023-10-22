/*
 * EECS 370, University of Michigan, Fall 2023
 * Project 3: LC-2K Pipeline Simulator
 * Instructions are found in the project spec: https://eecs370.github.io/project_3_spec/
 * Make sure NOT to modify printState or any of the associated functions
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define DEBUG_MODE

// Machine Definitions
#define NUMMEMORY 65536  // maximum number of data words in memory
#define NUMREGS 8        // number of machine registers

#define ADD 0
#define NOR 1
#define LW 2
#define SW 3
#define BEQ 4
#define JALR 5  // will not implemented for Project 3
#define HALT 6
#define NOOP 7

const char* opcode_to_str_map[] = {
    "add",
    "nor",
    "lw",
    "sw",
    "beq",
    "jalr",
    "halt",
    "noop"};

#define NOOPINSTR (NOOP << 22)

#define HAZ_1_REG_A 0b00000001
#define HAZ_1_REG_B 0b00000010
#define HAZ_2_REG_A 0b00000100
#define HAZ_2_REG_B 0b00001000
#define HAZ_3_REG_A 0b00010000
#define HAZ_3_REG_B 0b00100000

typedef struct DetectorStruct {
    int reg[3];
    int isLW;
} DetectorType;

typedef struct IFIDStruct {
    int pcPlus1;
    int instr;
} IFIDType;

typedef struct IDEXStruct {
    int pcPlus1;
    int valA;
    int valB;
    int offset;
    int instr;
    int hazard;
} IDEXType;

typedef struct EXMEMStruct {
    int branchTarget;
    int eq;
    int aluResult;
    int valB;
    int instr;
} EXMEMType;

typedef struct MEMWBStruct {
    int writeData;
    int instr;
} MEMWBType;

typedef struct WBENDStruct {
    int writeData;
    int instr;
} WBENDType;

typedef struct stateStruct {
    int pc;
    int instrMem[NUMMEMORY];
    int dataMem[NUMMEMORY];
    int reg[NUMREGS];
    unsigned int numMemory;
    IFIDType IFID;
    IDEXType IDEX;
    EXMEMType EXMEM;
    MEMWBType MEMWB;
    WBENDType WBEND;
    unsigned int cycles;  // number of cycles run so far
    DetectorType detector;
} stateType;

static inline int opcode(int instruction) {
    return instruction >> 22;
}

static inline int field0(int instruction) {
    return (instruction >> 19) & 0x7;
}

static inline int field1(int instruction) {
    return (instruction >> 16) & 0x7;
}

static inline int field2(int instruction) {
    return instruction & 0xFFFF;
}

// convert a 16-bit number into a 32-bit Linux integer
static inline int convertNum(int num) {
    return num - ((num & (1 << 15)) ? 1 << 16 : 0);
}

void printState(stateType*);
void printInstruction(int);
void readMachineCode(stateType*, char*);
void stageIF(IFIDType* pIFID,
             int* pPc_new,
             const int* pPc_old,
             const int instrMem[],
             const int* pBranchTarget);
void stageID(IDEXType* pIDEX,
             DetectorType* pDetector_new,
             const IFIDType* pIFID,
             const int reg[],
             const DetectorType* pDetector_old,
             int* pStall);
void stageEX(EXMEMType* pEXMEM,
             const IDEXType* pIDEX,
             const int* pData1,
             const int* pData2,
             const int* pData3);
void stageMEM(MEMWBType* pMEMWB,
              int dataMem_new[],
              const EXMEMType* pEXMEM,
              const int dataMem_old[]);
void stageWB(WBENDType* pWBEND,
             int reg[],
             const MEMWBType* pMEMWB);

int main(int argc, char* argv[]) {
    /* Declare state and newState.
       Note these have static lifetime so that instrMem and
       dataMem are not allocated on the stack. */

    static stateType state, newState;
    static int stall = 0;

    if (argc != 2) {
        printf("error: usage: %s <machine-code file>\n", argv[0]);
        exit(1);
    }

    readMachineCode(&state, argv[1]);

    // Initialize state here
    state.IFID.instr = 7 << 22;
    state.IDEX.instr = 7 << 22;
    state.EXMEM.instr = 7 << 22;
    state.MEMWB.instr = 7 << 22;
    state.WBEND.instr = 7 << 22;
    state.detector.reg[0] = -1;
    state.detector.reg[1] = -1;
    state.detector.reg[2] = -1;

    newState = state;

    while (opcode(state.MEMWB.instr) != HALT) {
        printState(&state);

        newState.cycles += 1;

        if (state.EXMEM.eq) {
            newState.EXMEM.instr = 7 << 22;
            newState.IDEX.instr = 7 << 22;
            newState.IFID.instr = 7 << 22;
            newState.pc = state.EXMEM.branchTarget;
            newState.EXMEM.eq = 0;
        } else {
            /* ---------------------- ID stage --------------------- */
            stageID(&newState.IDEX,
                    &newState.detector,
                    &state.IFID,
                    state.reg,
                    &state.detector,
                    &stall);

            /* ---------------------- IF stage --------------------- */
            if (!stall)
                stageIF(&newState.IFID,
                        &newState.pc,
                        &state.pc,
                        state.instrMem,
                        &state.EXMEM.branchTarget);
            stall = 0;

            /* ---------------------- EX stage --------------------- */
            stageEX(&newState.EXMEM,
                    &state.IDEX,
                    &state.EXMEM.aluResult,
                    &state.MEMWB.writeData,
                    &state.WBEND.writeData);
        }

        /* --------------------- MEM stage --------------------- */
        stageMEM(&newState.MEMWB,
                 newState.dataMem,
                 &state.EXMEM,
                 state.dataMem);

        /* ---------------------- WB stage --------------------- */
        stageWB(&newState.WBEND,
                newState.reg,
                &state.MEMWB);

        /* ------------------------ END ------------------------ */
        state = newState; /* this is the last statement before end of the loop. It marks the end
        of the cycle and updates the current state with the values calculated in this cycle */
    }
    printf("Machine halted\n");
    printf("Total of %d cycles executed\n", state.cycles);
    printf("Final state of machine:\n");
    printState(&state);
}

void stageIF(IFIDType* pIFID,
             int* pPc_new,
             const int* pPc_old,
             const int instrMem[],
             const int* pBranchTarget) {
    static int instr;
    static int op;
    static int addRes;
    static int muxRes;
    instr = instrMem[*pPc_old];
    op = opcode(instr);

    // if (1) // TODO: INSTR MEM EN?
    pIFID->instr = instr;
    addRes = *pPc_old + 1;
    // if (1) // TODO: BEQ?
    muxRes = addRes;
    // else
    //     muxResult = muxResult; // TODO: BEQ?
    // if (1) // TODO: PC EN?
    *pPc_new = muxRes;
    if (op != NOOP)
        pIFID->pcPlus1 = addRes;
}

void stageID(IDEXType* pIDEX,
             DetectorType* pDetector_new,
             const IFIDType* pIFID,
             const int reg[],
             const DetectorType* pDetector_old,
             int* pStall) {
    static int instr;
    static int op;
    static int regA, regB;
    instr = pIFID->instr;
    op = opcode(instr);
    regA = field0(instr);
    regB = field1(instr);
    pIDEX->instr = instr;
    // if (1) // TODO: REG EN?
    //     np->reg[0] = np->reg[0]; // TODO

    pIDEX->hazard = 0;

    if (((op < HALT && op >= 0 && regA == pDetector_old->reg[0]) ||
         (op != LW && op <= BEQ && op >= 0 && regB == pDetector_old->reg[0])) &&
        pDetector_old->isLW) {
        pIDEX->instr = 7 << 22;
        op == NOOP;
        *pStall = 1;
    } else {
        if (op < HALT && op >= 0) {
            if (regA == pDetector_old->reg[0])
                pIDEX->hazard |= HAZ_1_REG_A;
            if (regA == pDetector_old->reg[1])
                pIDEX->hazard |= HAZ_2_REG_A;
            if (regA == pDetector_old->reg[2])
                pIDEX->hazard |= HAZ_3_REG_A;
        }
        if (op != LW && op <= BEQ && op >= 0) {
            if (regB == pDetector_old->reg[0])
                pIDEX->hazard |= HAZ_1_REG_B;
            if (regB == pDetector_old->reg[1])
                pIDEX->hazard |= HAZ_2_REG_B;
            if (regB == pDetector_old->reg[2])
                pIDEX->hazard |= HAZ_3_REG_B;
        }

        if (op != NOOP)
            pIDEX->pcPlus1 = pIFID->pcPlus1;
        if (op < HALT && op >= 0)
            pIDEX->valA = reg[regA];
        if (op != LW && op <= BEQ && op >= 0)
            pIDEX->valB = reg[regB];
        pIDEX->offset = convertNum(field2(instr));
    }

    pDetector_new->reg[2] = pDetector_old->reg[1];
    pDetector_new->reg[1] = pDetector_old->reg[0];
    if (op == ADD || op == NOR) {
        pDetector_new->reg[0] = field2(instr);
        pDetector_new->isLW = 0;
    } else if (op == LW) {
        pDetector_new->reg[0] = regB;
        pDetector_new->isLW = 1;
    } else {
        pDetector_new->reg[0] = -1;
        pDetector_new->isLW = 0;
    }
}

void stageEX(EXMEMType* pEXMEM,
             const IDEXType* pIDEX,
             const int* pData1,
             const int* pData2,
             const int* pData3) {
    static int op;
    static int valA, valB;
    static int muxRes;
    op = opcode(pIDEX->instr);
    pEXMEM->instr = pIDEX->instr;

    if (pIDEX->hazard & HAZ_1_REG_A)
        valA = *pData1;
    else if (pIDEX->hazard & HAZ_2_REG_A)
        valA = *pData2;
    else if (pIDEX->hazard & HAZ_3_REG_A)
        valA = *pData3;
    else
        valA = pIDEX->valA;

    if (pIDEX->hazard & HAZ_1_REG_B)
        valB = *pData1;
    else if (pIDEX->hazard & HAZ_2_REG_B)
        valB = *pData2;
    else if (pIDEX->hazard & HAZ_3_REG_B)
        valB = *pData3;
    else
        valB = pIDEX->valB;

    if (op == SW || op == LW)
        muxRes = pIDEX->offset;
    else
        muxRes = valB;

    pEXMEM->branchTarget = pIDEX->pcPlus1 + pIDEX->offset;
    if (op == BEQ)
        pEXMEM->eq = valA == muxRes;
    else
        pEXMEM->eq = 0;

    if (op <= SW && op >= 0)
        if (op == NOR)
            pEXMEM->aluResult = ~(valA | muxRes);
        else
            pEXMEM->aluResult = valA + muxRes;

    if (op == SW)
        pEXMEM->valB = valB;
}

void stageMEM(MEMWBType* pMEMWB,
              int dataMem_new[],
              const EXMEMType* pEXMEM,
              const int dataMem_old[]) {
    static int op;
    op = opcode(pEXMEM->instr);
    pMEMWB->instr = pEXMEM->instr;
    if (op < SW && op >= 0)
        if (op == LW)
            pMEMWB->writeData = dataMem_old[pEXMEM->aluResult];
        else
            pMEMWB->writeData = pEXMEM->aluResult;
    if (op == SW)
        dataMem_new[pEXMEM->aluResult] = pEXMEM->valB;
}

void stageWB(WBENDType* pWBEND,
             int reg[],
             const MEMWBType* pMEMWB) {
    static int instr;
    static int op;
    static int muxResult;
    instr = pMEMWB->instr;
    pWBEND->instr = instr;
    op = opcode(instr);
    if (op == LW)
        muxResult = field1(instr);
    else
        muxResult = field2(instr);
    if (op < SW && op >= 0)
        reg[muxResult] = pMEMWB->writeData;
    pWBEND->writeData = pMEMWB->writeData;
}

/*
 * DO NOT MODIFY ANY OF THE CODE BELOW.
 */

void printInstruction(int instr) {
    const char* instr_opcode_str;
    int instr_opcode = opcode(instr);
    if (ADD <= instr_opcode && instr_opcode <= NOOP) {
        instr_opcode_str = opcode_to_str_map[instr_opcode];
    }

    switch (instr_opcode) {
        case ADD:
        case NOR:
        case LW:
        case SW:
        case BEQ:
            printf("%s %d %d %d", instr_opcode_str, field0(instr), field1(instr), convertNum(field2(instr)));
            break;
        case JALR:
            printf("%s %d %d", instr_opcode_str, field0(instr), field1(instr));
            break;
        case HALT:
        case NOOP:
            printf("%s", instr_opcode_str);
            break;
        default:
            printf(".fill %d", instr);
            return;
    }
}

void printState(stateType* statePtr) {
    printf("\n@@@\n");
    printf("state before cycle %d starts:\n", statePtr->cycles);
    printf("\tpc = %d\n", statePtr->pc);

    printf("\tdata memory:\n");
    for (int i = 0; i < statePtr->numMemory; ++i) {
        printf("\t\tdataMem[ %d ] = %d\n", i, statePtr->dataMem[i]);
    }
    printf("\tregisters:\n");
    for (int i = 0; i < NUMREGS; ++i) {
        printf("\t\treg[ %d ] = %d\n", i, statePtr->reg[i]);
    }

    // IF/ID
    printf("\tIF/ID pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IFID.instr);
    printInstruction(statePtr->IFID.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IFID.pcPlus1);
    if (opcode(statePtr->IFID.instr) == NOOP) {
        printf(" (Don't Care)");
    }
    printf("\n");

#ifdef DEBUG_MODE
    printf("\t[[Detector: %d, %d, %d | LW: %d]]\n",
           statePtr->detector.reg[0],
           statePtr->detector.reg[1],
           statePtr->detector.reg[2],
           statePtr->detector.isLW);
#endif

    // ID/EX
    int idexOp = opcode(statePtr->IDEX.instr);
    printf("\tID/EX pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IDEX.instr);
    printInstruction(statePtr->IDEX.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IDEX.pcPlus1);
    if (idexOp == NOOP) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegA = %d", statePtr->IDEX.valA);
    if (idexOp >= HALT || idexOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->IDEX.valB);
    if (idexOp == LW || idexOp > BEQ || idexOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\toffset = %d", statePtr->IDEX.offset);
    if (idexOp != LW && idexOp != SW && idexOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");

#ifdef DEBUG_MODE
    printf("\t[[Hazard: 0x%x]]\n", statePtr->IDEX.hazard);
#endif

    // EX/MEM
    int exmemOp = opcode(statePtr->EXMEM.instr);
    printf("\tEX/MEM pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->EXMEM.instr);
    printInstruction(statePtr->EXMEM.instr);
    printf(" )\n");
    printf("\t\tbranchTarget %d", statePtr->EXMEM.branchTarget);
    if (exmemOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\teq ? %s", (statePtr->EXMEM.eq ? "True" : "False"));
    if (exmemOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\taluResult = %d", statePtr->EXMEM.aluResult);
    if (exmemOp > SW || exmemOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->EXMEM.valB);
    if (exmemOp != SW) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // MEM/WB
    int op = opcode(statePtr->MEMWB.instr);
    printf("\tMEM/WB pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->MEMWB.instr);
    printInstruction(statePtr->MEMWB.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->MEMWB.writeData);
    if (op >= SW || op < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // WB/END
    int wbendOp = opcode(statePtr->WBEND.instr);
    printf("\tWB/END pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->WBEND.instr);
    printInstruction(statePtr->WBEND.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->WBEND.writeData);
    if (wbendOp >= SW || wbendOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");

    printf("end state\n");
    fflush(stdout);
}

// File
#define MAXLINELENGTH 1000  // MAXLINELENGTH is the max number of characters we read

void readMachineCode(stateType* state, char* filename) {
    char line[MAXLINELENGTH];
    FILE* filePtr = fopen(filename, "r");
    if (filePtr == NULL) {
        printf("error: can't open file %s", filename);
        exit(1);
    }

    printf("instruction memory:\n");
    for (state->numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL; ++state->numMemory) {
        if (sscanf(line, "%d", state->instrMem + state->numMemory) != 1) {
            printf("error in reading address %d\n", state->numMemory);
            exit(1);
        }
        printf("\tinstrMem[ %d ]\t= 0x%08x\t= %d\t= ", state->numMemory,
               state->instrMem[state->numMemory], state->instrMem[state->numMemory]);
        printInstruction(state->dataMem[state->numMemory] = state->instrMem[state->numMemory]);
        printf("\n");
    }
}
