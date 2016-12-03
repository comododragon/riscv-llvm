def ADDV : InstR<"addv" , 0b0001011, 0b0000000, 0b000, add   , GR32, GR32>, Requires<[IsRV32]>;
def SUBV : InstR<"subv" , 0b0001011, 0b0100000, 0b000, sub   , GR32, GR32>, Requires<[IsRV32]>;
def SLLV : InstR<"sllv" , 0b0001011, 0b0000000, 0b001, shl   , GR32, GR32>, Requires<[IsRV32]>;
def SLTV : InstR<"sltv" , 0b0001011, 0b0000000, 0b010, setlt , GR32, GR32>;
def SLTUV: InstR<"sltuv", 0b0001011, 0b0000000, 0b011, setult, GR32, GR32>;
def XORV : InstR<"xorv" , 0b0001011, 0b0000000, 0b100, xor   , GR32, GR32>;
def SRLV : InstR<"srlv" , 0b0001011, 0b0000000, 0b101, srl   , GR32, GR32>, Requires<[IsRV32]>;
def SRAV : InstR<"srav" , 0b0001011, 0b0100000, 0b101, sra   , GR32, GR32>, Requires<[IsRV32]>;
def ORV  : InstR<"orv"  , 0b0001011, 0b0000000, 0b110, or    , GR32, GR32>;
def ANDV : InstR<"andv" , 0b0001011, 0b0000000, 0b111, and   , GR32, GR32>;
//Integer arithmetic register-immediate
def ADDIV: InstI<"addiv", 0b0101011, 0b000       , add, GR32, GR32, imm32sx12>, Requires<[IsRV32]>;
def XORIV: InstI<"xoriv", 0b0101011, 0b100       , xor, GR32, GR32, imm32sx12>;
def ORIV : InstI<"oriv" , 0b0101011, 0b110       , or , GR32, GR32, imm32sx12>;
def ANDIV: InstI<"andiv", 0b0101011, 0b111       , and, GR32, GR32, imm32sx12>;

//TODO: enforce constraints here or up on level?
def SLLIV: InstI<"slliv", 0b0101011, 0b001       , shl, GR32, GR32, imm32sx12>, Requires<[IsRV32]>{
  let IMM{11-6} = 0b000000; 
  //trap if $imm{5}!=0 TODO:how to do this?
}
def SRLIV: InstI<"srliv", 0b0101011, 0b101       , srl, GR32, GR32, imm32sx12>, Requires<[IsRV32]>{
  let IMM{11-6} = 0b000000; 
  //trap if $src{5}!=0 TODO:how to do this?
}
def SRAIV: InstI<"sraiv", 0b0101011, 0b101       , sra, GR32, GR32, imm32sx12>, Requires<[IsRV32]>{
  let IMM{11-6} = 0b010000;
  //trap if $src{5}!=0 TODO:how to do this?
}
def SLTIV : InstI<"sltiv", 0b0101011, 0b010, setlt, GR32, GR32, imm32sx12>;
def SLTIUV: InstI<"sltiuv",0b0101011, 0b011, setult,GR32, GR32, imm32sx12>;
