/*
 * EECS 370, University of Michigan, Fall 2023
 * Project 3: LC-2K Pipeline Simulator
 * Instructions are found in the project spec: https://eecs370.github.io/project_3_spec/
 * Make sure NOT to modify printState or any of the associated functions
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
int getRegValue(stateType*, int, int);
int isRegUsed(int, int);

int main(int argc, char* argv[]) {
    /* Declare state and newState.
       Note these have static lifetime so that instrMem and
       dataMem are not allocated on the stack. */

    static stateType state, newState;

    if (argc != 2) {
        printf("error: usage: %s <machine-code file>\n", argv[0]);
        exit(1);
    }

    readMachineCode(&state, argv[1]);

    // Initialize state here

    state.cycles = 0;
    memset(state.reg, 0, sizeof(state.reg));
    state.pc = 0;
    state.IFID.instr = NOOPINSTR;
    state.IDEX.instr = NOOPINSTR;
    state.EXMEM.instr = NOOPINSTR;
    state.MEMWB.instr = NOOPINSTR;
    state.WBEND.instr = NOOPINSTR;

    newState = state;

    while (opcode(state.MEMWB.instr) != HALT) {
        printState(&state);

        newState = state;

        newState.cycles += 1;

        /* ---------------------- IF stage --------------------- */

        newState.IFID.instr = state.instrMem[state.pc];
        newState.IFID.pcPlus1 = state.pc + 1;

        newState.pc = state.pc + 1;

        /* ---------------------- ID stage --------------------- */
        // You will need to stall for one type of data hazard: a lw followed by an instruction that uses the register being loaded.

        if (opcode(state.IDEX.instr) == LW && isRegUsed(state.IFID.instr, field1(state.IDEX.instr))) {
            newState.IDEX.instr = NOOPINSTR;
            newState.pc = state.pc;
            newState.IFID = state.IFID;
        } else {
            newState.IDEX.instr = state.IFID.instr;
            newState.IDEX.valA = state.reg[field0(state.IFID.instr)];
            newState.IDEX.valB = state.reg[field1(state.IFID.instr)];
            newState.IDEX.pcPlus1 = state.IFID.pcPlus1;
            newState.IDEX.offset = convertNum(field2(state.IFID.instr));
        }

        /* ---------------------- EX stage --------------------- */
        /*
        Use data forwarding to resolve most data hazards.
        The ALU should be able to take its inputs from any pipeline register
        (instead of just the IDEX register).
        To account for a lack of internal forwarding within the register file,
        you’ll instead forward data from the new WBEND pipeline register.
        Remember to take the most recent data
        (e.g., data in the EXMEM register gets priority over data in the MEMWB register).
        ONLY FORWARD DATA TO THE EX STAGE (not to memory).
        */

        newState.EXMEM.branchTarget = state.IDEX.pcPlus1 + state.IDEX.offset;
        int alu1In = getRegValue(&state, field0(state.IDEX.instr), state.IDEX.valA), alu2In = 0;
        if (opcode(state.IDEX.instr) <= NOR || opcode(state.IDEX.instr) == BEQ)
            alu2In = getRegValue(&state, field1(state.IDEX.instr), state.IDEX.valB);
        else
            alu2In = state.IDEX.offset;
        newState.EXMEM.eq = (alu2In == alu1In);
        int aluOp = opcode(state.IDEX.instr) == NOR;
        if (aluOp == 0)
            newState.EXMEM.aluResult = alu1In + alu2In;
        else
            newState.EXMEM.aluResult = ~(alu2In | alu1In);
        newState.EXMEM.valB = getRegValue(&state, field1(state.IDEX.instr), state.IDEX.valB);
        newState.EXMEM.instr = state.IDEX.instr;
        // printf("========================= ALU: %d %d %d\n", alu1In, alu2In, alu1In == alu2In);

        /* --------------------- MEM stage --------------------- */
        /*Predict branch-not-taken to speculate on branches,
        and decide whether or not to take the branch in the MEM stage.
        This requires you to discard instructions if it turns out
        that the branch prediction was incorrect.
        To discard instructions, change the relevant instructions
        in the pipeline to the noop instruction (0x1c00000).
         */
        int opMem = opcode(state.EXMEM.instr);
        if (opMem == SW) {
            newState.MEMWB.writeData = state.EXMEM.valB;
            // newState.dataMem[state.EXMEM.aluResult] = state.EXMEM.valB;
        } else if (opMem == LW) {
            newState.MEMWB.writeData = state.dataMem[state.EXMEM.aluResult];
        } else if (opMem <= NOR) {
            newState.MEMWB.writeData = state.EXMEM.aluResult;
        }
        if (opMem == BEQ && state.EXMEM.eq) {
            newState.pc = state.EXMEM.branchTarget;
            newState.IFID.instr = NOOPINSTR;
            newState.IDEX.instr = NOOPINSTR;
            newState.EXMEM.instr = NOOPINSTR;
        }
        newState.MEMWB.instr = state.EXMEM.instr;
        /* ---------------------- WB stage --------------------- */
        // the starter code stops when the halt instruction reaches the MEMWB register.
        int opWb = opcode(state.MEMWB.instr);
        if (opWb == LW) {
            newState.reg[field1(state.MEMWB.instr)] = state.MEMWB.writeData;
        } else if (opWb == SW) {
            newState.dataMem[state.reg[field0(state.MEMWB.instr)] + field2(state.MEMWB.instr)] = state.MEMWB.writeData;
        } else if (opWb <= NOR) {
            newState.reg[field2(state.MEMWB.instr)] = state.MEMWB.writeData;
        }
        newState.WBEND.writeData = state.MEMWB.writeData;
        newState.WBEND.instr = state.MEMWB.instr;

        /* ------------------------ END ------------------------ */
        state = newState; /* this is the last statement before end of the loop. It marks the end
        of the cycle and updates the current state with the values calculated in this cycle */
    }
    printf("Machine halted\n");
    printf("Total of %d cycles executed\n", state.cycles);
    printf("Final state of machine:\n");
    printState(&state);
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
    int memwbOp = opcode(statePtr->MEMWB.instr);
    printf("\tMEM/WB pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->MEMWB.instr);
    printInstruction(statePtr->MEMWB.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->MEMWB.writeData);
    if (memwbOp >= SW || memwbOp < 0) {
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

int getRegValue(stateType* state, int reg, int now) {
    if (opcode(state->EXMEM.instr) <= NOR &&
        field2(state->EXMEM.instr) == reg) {
        return state->EXMEM.aluResult;
    }
    if (opcode(state->MEMWB.instr) <= NOR &&
        field2(state->MEMWB.instr) == reg) {
        return state->MEMWB.writeData;
    }
    if (opcode(state->MEMWB.instr) == LW &&
        field1(state->MEMWB.instr) == reg) {
        return state->MEMWB.writeData;
    }
    if (opcode(state->WBEND.instr) <= NOR &&
        field2(state->WBEND.instr) == reg) {
        return state->WBEND.writeData;
    }
    if (opcode(state->WBEND.instr) == LW &&
        field1(state->WBEND.instr) == reg) {
        return state->WBEND.writeData;
    }

    return now;
}

int isRegUsed(int instr, int reg) {
    int opc = opcode(instr);
    // printf("===========================%d %d %d\n", field0(instr), field1(instr), reg);
    switch (opc) {
        case ADD:
        case NOR:
        case BEQ:
        case SW:
        case LW:
            return field0(instr) == reg || field1(instr) == reg;
        // case LW:
        //     return field0(instr) == reg;
        default:
            break;
    }
    return 0;
}
