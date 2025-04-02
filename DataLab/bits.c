/* 
 * CS:APP Data Lab 
 * 
 * <Please put your name and userid here>
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implent floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
/* Copyright (C) 1991-2022 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */
/* This header is separate from features.h so that the compiler can
   include it implicitly at the start of every compilation.  It must
   not itself include <features.h> or any other header that includes
   <features.h> because the implicit include comes before any feature
   test macros that may be defined in a source file before it first
   explicitly includes a system header.  GCC knows the name of this
   header in order to preinclude it.  */
/* glibc's intent is to support the IEC 559 math functionality, real
   and complex.  If the GCC (4.9 and later) predefined macros
   specifying compiler intent are available, use them to determine
   whether the overall intent is to support these features; otherwise,
   presume an older compiler has intent to support these features and
   define these macros by default.  */
/* wchar_t uses Unicode 10.0.0.  Version 10.0 of the Unicode Standard is
   synchronized with ISO/IEC 10646:2017, fifth edition, plus
   the following additions from Amendment 1 to the fifth edition:
   - 56 emoji characters
   - 285 hentaigana
   - 3 additional Zanabazar Square characters */
/* 
 * bitAnd - x&y using only ~ and | 
 *   Example: bitAnd(6, 5) = 4
 *   Legal ops: ~ |
 *   Max ops: 8
 *   Rating: 1
 */
int bitAnd(int x, int y) {
  return ~((~x)|(~y));
}
/* 
 * bitConditional - x ? y : z for each bit respectively
 *   Example: bitConditional(0b00110011, 0b01010101, 0b00001111) = 0b00011101
 *   Legal ops: & | ^ ~
 *   Max ops: 4
 *   Rating: 1
 */
int bitConditional(int x, int y, int z) {
  return (x&y)|(~x&z);
}
/* 
 * implication - return x -> y in propositional logic - 0 for false, 1
 * for true
 *   Example: implication(1,1) = 1
 *            implication(1,0) = 0
 *   Legal ops: ! ~ ^ |
 *   Max ops: 5
 *   Rating: 2
 */
int implication(int x, int y) {
    return (!x)|(!!y);
}
/* 
 * rotateRight - Rotate x to the right by n
 *   Can assume that 0 <= n <= 31
 *   Examples: rotateRight(0x87654321,4) = 0x18765432
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 25
 *   Rating: 3 
 */
int rotateRight(int x, int n) {
  int move_to_left = x<<(32+(~n+1)); //   -n=~n+1, move to the left
  int move_to_right_1 = x>>n; // arithmetic shift may start with 0 or 1
  int Det=~((~1+1)<<(32+(~n+1))); // make a determination get 111110000
  int move_to_right_2 = move_to_right_1 & Det ; // get the right shifted start with 0
  return move_to_left|move_to_right_2; // add together
}
/* 
 * bang - Compute !x without using !
 *   Examples: bang(3) = 0, bang(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4 
 */
int bang(int x) {
  return (((~x+1)|x)>>31)+1;
}
/* 
 * countTrailingZero - return the number of consecutive 0 from the lowest bit of 
 *   the binary form of x.
 *   YOU MAY USE BIG CONST IN THIS PROBLEM, LIKE 0xFFFF0000
 *   YOU MAY USE BIG CONST IN THIS PROBLEM, LIKE 0xFFFF0000
 *   YOU MAY USE BIG CONST IN THIS PROBLEM, LIKE 0xFFFF0000
 *   Examples countTrailingZero(0x0) = 32, countTrailingZero(0x1) = 0,
 *            countTrailingZero(0xFFFF0000) = 16,
 *            countTrailingZero(0xFFFFFFF0) = 8,
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 40
 *   Rating: 4
 */
int countTrailingZero(int x){
  int Re=0;
  int F_Num;
  //int Byte_of_x;
  int Det_if_Zero=0;
  int Final_Check;
  int Det;
  F_Num= 0xFF;//  0000 00FF get  0000 00X
  Det_if_Zero=!(F_Num&x); // Det_if_Zero =0 => X has no 1 | Det=1 => X has 1
  Re+=Det_if_Zero<<3; // Re=8;
  F_Num= 0xFFFF;//  0000 FFFF get  0000 X
  Det_if_Zero=!(F_Num&x); // Det=0 => X has no 1 | Det=1 => X has 1
  Re+=Det_if_Zero<<3;  //Re=16
  F_Num= 0xFFFFFF;//  00FF FFFF get  00X 
  Det_if_Zero=!(F_Num&x); // Det=0 => X has no 1 | Det=1 => X has 1
  Re+=Det_if_Zero<<3; //Re=24=2^4 (16)+ 2^3 (8)
  //F_Num= 0xFFFFFFFF;//  FFFF FFFF get  X
  Det_if_Zero=!x; // Det=0 => X has no 1 | Det=1 => X has 1
  Re+=Det_if_Zero<<3; //Re=32
  
  Det=!(Re+(~32)+1);//Det =1 when Re=32 / else Det =0;
  // if Re =32 (det=1) let final = -1 ,else(det=0)  let final>>Re; 
  Final_Check=(x>>Re)|Det;
  F_Num=0xF;//F
  Det_if_Zero=!(Final_Check&F_Num);
  Re+=Det_if_Zero<<2;
  Final_Check>>=(Det_if_Zero<<2);
  F_Num=0x1;//0001
  Det_if_Zero=!(Final_Check&F_Num);
  Re+=Det_if_Zero;
  F_Num=0x3;//0011
  Det_if_Zero=!(Final_Check&F_Num);
  Re+=Det_if_Zero;
  F_Num=0x7;//0111
  Det_if_Zero=!(Final_Check&F_Num);
  Re+=Det_if_Zero;
  F_Num=0xF;//1111
  Det_if_Zero=!(Final_Check&F_Num);
  Re+=Det_if_Zero;
  
  return Re;
}
/* 
 * divpwr2 - Compute x/(2^n), for 0 <= n <= 30
 *  Round toward zero
 *   Examples: divpwr2(15,1) = 7, divpwr2(-33,4) = -2
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 2
 */
int divpwr2(int x, int n) {
  int Det_Neg=((1<<n)+(~0))&(x>>31);
  return (x+Det_Neg)>>n;
  //return (x>>n)|(~(x>>31)+1);
  
}
/* 
 * sameSign - return 1 if x and y have same sign, and 0 otherwise
 *   Examples sameSign(0x12345678, 0) = 1, sameSign(0xFFFFFFFF,0x1) = 0
 *   Legal ops: ! ~ & ! ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int sameSign(int x, int y) {
  //int Det1=~(x>>31)+1;
  //int Det2=~(y>>31)+1;
  return !((x>>31)^(y>>31));
}
/*
 * multFiveEighths - multiplies by 5/8 rounding toward 0.
 *   Should exactly duplicate effect of C expression (x*5/8),
 *   including overflow behavior.
 *   Examples: multFiveEighths(77) = 48
 *             multFiveEighths(-22) = -13
 *             multFiveEighths(1073741824) = 13421728 (overflow)
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 3
 */
int multFiveEighths(int x) {
  //return ((x<<2)+x)>>3;
  int Multiply=(x<<2)+x; 
  int Det_Neg=((1<<3)+(~0))&(Multiply>>31);
  return (Multiply+Det_Neg)>>3;
  //return (((x<<2)+x)+(((1<<3)+(~0))&(x>>31)))>>3;
}
/*
 * satMul3 - multiplies by 3, saturating to Tmin or Tmax if overflow
 *  Examples: satMul3(0x10000000) = 0x30000000
 *            satMul3(0x30000000) = 0x7FFFFFFF (Saturate to TMax)
 *            satMul3(0x70000000) = 0x7FFFFFFF (Saturate to TMax)
 *            satMul3(0xD0000000) = 0x80000000 (Saturate to TMin)
 *            satMul3(0xA0000000) = 0x80000000 (Saturate to TMin)
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 25
 *  Rating: 3
 */
int satMul3(int x) {
    int A = x<<1;
    int B = A+x;
    int sign_of_x = x>>31; //get x'sign 1111 1111 | 0000 0000
    int Check_A=(x^A)>>31; // -1=>overflow | 0=>notoverflow ;
    int Check_B=(x^B)>>31; // -1=>overflow | 0=>notoverflow ;
    int Chek_Over=(Check_A|Check_B); //final check overflow -1=>overflow | 0=>notoverflow
    int M = sign_of_x^(1<<31); // 1111 1111(negative)  | 0000 0000 (^)  1000 0000 = 0111 1111 | 1000 0000 // negative = TMax, or = TMin;
    return (Chek_Over&M)^(Chek_Over|B);
}
/* 
 * isLessOrEqual - if x <= y  then return 1, else return 0 
 *   Example: isLessOrEqual(4,5) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isLessOrEqual(int x, int y) {
  return ((x&~y) | ~((x^y)|(~x+y+1)))>>31&1;
  //return x^y;
}
/*
 * ilog2 - return floor(log base 2 of x), where x > 0
 *   Example: ilog2(16) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 90
 *   Rating: 4
 */
int ilog2(int x) {
  int Re = 0;
  Re = (!!(x >> 16)) << 4; //if(x)
  Re += ((!!(x >>(Re+8)))<< 3);
  Re += ((!!(x >>(Re+4)))<< 2);
  Re += ((!!(x >>(Re+2)))<< 1);
  Re += (!!(x>>(Re+1)));
  return Re;
  //return 2;
}
/* 
 * float_twice - Return bit-level equivalent of expression 2*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_twice(unsigned uf) {
  const unsigned A = 0xff800000;  
  unsigned Det = A&uf;

  switch (Det){
    case 0x7f800000:
    case 0xff800000:  return uf;
    case 0x00000000:           
    case 0x80000000:  return Det|(uf<<1);
    case 0x7f000000:  return 0x7f800000;
    case 0xff000000:  return 0xff800000;
    default:          return uf+0x800000;
  }
  //return 2;
}
/* 
 * float_i2f - Return bit-level equivalent of expression (float) x
 *   Result is returned as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point values.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_i2f(int x) {
  unsigned s = 0, Exp = 31, Frac = 0, Det = 0;
  if (x == 0x00000000u) return 0;
  if (x & 0x80000000u) { s = 0x80000000u; x = -x; }
  while (1) {
    if (x & 0x80000000u) break;
    Exp -= 1;
    x <<= 1;
  }
  if ((x & 0x000001ff) == 0x180) Det = 1;
  else if ((x & 0xff) > 0x80) Det = 1;
  Frac = ((x & 0x7fffffffu) >> 8) + Det;
  return s + ((Exp + 127) << 23) + Frac;
  //return 2;
}
/* 
 * float64_f2i - Return bit-level equivalent of expression (int) f
 *   for 64 bit floating point argument f.
 *   Argument is passed as two unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   double-precision floating point value.
 *   Notice: uf1 contains the lower part of the f64 f
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 20
 *   Rating: 4
 */
int float64_f2i(unsigned uf1, unsigned uf2) {
    unsigned exp = uf2 >> 20 & 0x7FF;
  int E = exp - 1023;
  unsigned sign = uf2 >> 31;
  unsigned frac = (uf2 & 0xFFFFF) | 0x100000;
  unsigned result;
  if (exp >= 0x7FF || E > 30) return 0x80000000u;
  if (E < 0) return 0;
  if (E > 20)
    result = frac << (E - 20) | uf1 >> (52 - E);
  else
    result = frac >> (20 - E);
  if (sign)
    return E == 31 ? 0x80000000u : -result;
  return result;
}
/* 
 * float_negpwr2 - Return bit-level equivalent of the expression 2.0^-x

 *
 *   The unsigned value that is returned should have the identical bit
 *   representation as the single-precision floating-point number 2.0^-x.
 *   If the result is too small to be represented as a denorm, return
 *   0. If too large, return +INF.
 * 
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. Also if, while 
 *   Max ops: 20 
 *   Rating: 4
 */
unsigned float_negpwr2(int x) {
  unsigned Exp;
  int Frac;
  if(x<-127) return 0xff<<23;
  if(x>149)  return 0;
  if(x>=-127&&x<127){
    Exp=-x+127;
    return Exp<<23;
  }
  if(x>=127&&x<=149){
    Frac=-x+126;
    return 1<<(23+Frac);
  }
  return 0;
}
