//郑斗薰 2100094820
/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    
    // 处理 32x32 的情况
    if (M == 32 && N == 32) { 
    int blk_x, blk_y, row, col; 
    int diag_val, diag_idx; 
        for (blk_y = 0; blk_y < 4; blk_y++) { 
            for (blk_x = 0; blk_x < 4; blk_x++) { 
                for (row = blk_y * 8; row < blk_y * 8 + 8; row++) {
                    for (col = blk_x * 8; col < blk_x * 8 + 8; col++) { 
                        if (row != col) {
                            B[col][row] = A[row][col]; // 非对角线直接转置
                        } else {
                            diag_val = A[row][col];  // 缓存对角线元素
                            diag_idx = row;         // 记录位置
                        }
                    }
                    if (blk_x == blk_y) { 
                        B[diag_idx][diag_idx] = diag_val; // 更新对角线
                    }
                }
            }
        }
    } 
    // 处理 64x64 的情况
    else if (M == 64 && N == 64) {
    int block_start_row, block_start_col, inner_row, inner_col;
    int reg1, reg2, reg3, reg4, reg5, reg6, reg7, reg8;

    for (block_start_row = 0; block_start_row < N; block_start_row += 8) { 
        for (block_start_col = 0; block_start_col < M; block_start_col += 8) { 
            for (inner_row = block_start_row; inner_row < block_start_row + 4; inner_row++) {
                reg1 = A[inner_row][block_start_col + 0];
                reg2 = A[inner_row][block_start_col + 1];
                reg3 = A[inner_row][block_start_col + 2];
                reg4 = A[inner_row][block_start_col + 3];
                reg5 = A[inner_row][block_start_col + 4];
                reg6 = A[inner_row][block_start_col + 5];
                reg7 = A[inner_row][block_start_col + 6];
                reg8 = A[inner_row][block_start_col + 7];
                B[block_start_col + 0][inner_row] = reg1;
                B[block_start_col + 1][inner_row] = reg2;
                B[block_start_col + 2][inner_row] = reg3;
                B[block_start_col + 3][inner_row] = reg4;
                B[block_start_col + 0][inner_row + 4] = reg5;
                B[block_start_col + 1][inner_row + 4] = reg6;
                B[block_start_col + 2][inner_row + 4] = reg7;
                B[block_start_col + 3][inner_row + 4] = reg8;
            }

            for (inner_col = block_start_col; inner_col < block_start_col + 4; inner_col++) {
                reg1 = A[block_start_row + 4][inner_col];
                reg2 = A[block_start_row + 5][inner_col];
                reg3 = A[block_start_row + 6][inner_col];
                reg4 = A[block_start_row + 7][inner_col];
                reg5 = B[inner_col][block_start_row + 4];
                reg6 = B[inner_col][block_start_row + 5];
                reg7 = B[inner_col][block_start_row + 6];
                reg8 = B[inner_col][block_start_row + 7];
                B[inner_col][block_start_row + 4] = reg1;
                B[inner_col][block_start_row + 5] = reg2;
                B[inner_col][block_start_row + 6] = reg3;
                B[inner_col][block_start_row + 7] = reg4;
                B[block_start_col + 4 + inner_col - block_start_col][block_start_row] = reg5;
                B[block_start_col + 4 + inner_col - block_start_col][block_start_row + 1] = reg6;
                B[block_start_col + 4 + inner_col - block_start_col][block_start_row + 2] = reg7;
                B[block_start_col + 4 + inner_col - block_start_col][block_start_row + 3] = reg8;
            }

            for (inner_row = block_start_row + 4; inner_row < block_start_row + 8; inner_row++) {
                reg1 = A[inner_row][block_start_col + 4];
                reg2 = A[inner_row][block_start_col + 5];
                reg3 = A[inner_row][block_start_col + 6];
                reg4 = A[inner_row][block_start_col + 7];
                B[block_start_col + 4][inner_row] = reg1;
                B[block_start_col + 5][inner_row] = reg2;
                B[block_start_col + 6][inner_row] = reg3;
                B[block_start_col + 7][inner_row] = reg4;
            }
        }
}
    }
    // 处理 60x68 的情况
    else if (M == 60 && N == 68) {
    int row, col; 
        for (row = 0; row < N; row += 16) {
            for (col = 0; col < M; col += 16) {
                for (int r = row; r < row + 16 && r < N; r++) {
                    for (int c = col; c < col + 16 && c < M; c++) {
                        B[c][r] = A[r][c];
                    }
                }
            }
        }
    }
}
/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

 /*
  * trans - A simple baseline transpose function, not optimized for the cache.
  */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);

}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

