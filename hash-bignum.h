/**
 * \file bignum.h
 */
#ifndef XYSSL_BIGNUM_H
#define XYSSL_BIGNUM_H

#define XYSSL_ERR_MPI_FILE_IO_ERROR                     -0x0002
#define XYSSL_ERR_MPI_BAD_INPUT_DATA                    -0x0004
#define XYSSL_ERR_MPI_INVALID_CHARACTER                 -0x0006
#define XYSSL_ERR_MPI_BUFFER_TOO_SMALL                  -0x0008
#define XYSSL_ERR_MPI_NEGATIVE_VALUE                    -0x000A
#define XYSSL_ERR_MPI_DIVISION_BY_ZERO                  -0x000C
#define XYSSL_ERR_MPI_NOT_ACCEPTABLE                    -0x000E

#define MPI_CHK(f) if((ret = f) != 0) goto cleanup

/**
 * \brief          MPI structure
 */
typedef struct {
    int s;              /*!<  integer sign      */
    int n;              /*!<  total # of limbs  */
    uint32_t *p;        /*!<  pointer to limbs  */
} mpi_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          Initialize one or more mpi_t
 */
void mpi_init(mpi_t *X, ...);

/**
 * \brief          Unallocate one or more mpi_t
 */
void mpi_free(mpi_t *X, ...);

/**
 * \brief          Enlarge to the specified number of limbs
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_grow(mpi_t *X, int nblimbs);

/**
 * \brief          Copy the contents of Y into X
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_copy(mpi_t *X, mpi_t *Y);

/**
 * \brief          Swap the contents of X and Y
 */
void mpi_swap(mpi_t *X, mpi_t *Y);

/**
 * \brief          Set value from integer
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_lset(mpi_t *X, int z);

/**
 * \brief          Return the number of least significant bits
 */
int mpi_lsb(mpi_t *X);

/**
 * \brief          Return the number of most significant bits
 */
int mpi_msb(mpi_t *X);

/**
 * \brief          Return the total size in bytes
 */
int mpi_size(mpi_t *X);

/**
 * \brief          Import from an ASCII string
 *
 * \param X        destination mpi_t
 * \param radix    input numeric base
 * \param s        null-terminated string buffer
 *
 * \return         0 if successful, or an XYSSL_ERR_MPI_XXX error code
 */
int mpi_read_string(mpi_t *X, int radix, const char *s);

/**
 * \brief          Export into an ASCII string
 *
 * \param X        source mpi_t
 * \param radix    output numeric base
 * \param s        string buffer
 * \param slen     string buffer size
 *
 * \return         0 if successful, or an XYSSL_ERR_MPI_XXX error code
 *
 * \note           Call this function with *slen = 0 to obtain the
 *                 minimum required buffer size in *slen.
 */
int mpi_write_string(mpi_t *X, int radix, char *s, int *slen);

/**
 * \brief          Read X from an opened file
 *
 * \param X        destination mpi_t
 * \param radix    input numeric base
 * \param fin      input file handle
 *
 * \return         0 if successful, or an XYSSL_ERR_MPI_XXX error code
 */
int mpi_read_file(mpi_t *X, int radix, FILE *fin);

/**
 * \brief          Write X into an opened file, or stdout
 *
 * \param p        prefix, can be NULL
 * \param X        source mpi_t
 * \param radix    output numeric base
 * \param fout     output file handle
 *
 * \return         0 if successful, or an XYSSL_ERR_MPI_XXX error code
 *
 * \note           Set fout == NULL to print X on the console.
 */
int mpi_write_file(const char *p, mpi_t *X, int radix, FILE *fout);

/**
 * \brief          Import X from unsigned binary data, big endian
 *
 * \param X        destination mpi_t
 * \param buf      input buffer
 * \param buflen   input buffer size
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_read_binary(mpi_t *X, const byte *buf, int buflen);

/**
 * \brief          Export X into unsigned binary data, big endian
 *
 * \param X        source mpi_t
 * \param buf      output buffer
 * \param buflen   output buffer size
 *
 * \return         0 if successful,
 *                 XYSSL_ERR_MPI_BUFFER_TOO_SMALL if buf isn't large enough
 *
 * \note           Call this function with *buflen = 0 to obtain the
 *                 minimum required buffer size in *buflen.
 */
int mpi_write_binary(mpi_t *X, byte *buf, int buflen);

/**
 * \brief          Left-shift: X <<= count
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_shift_l(mpi_t *X, int count);

/**
 * \brief          Right-shift: X >>= count
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_shift_r(mpi_t *X, int count);

/**
 * \brief          Compare unsigned values
 *
 * \return         1 if |X| is greater than |Y|,
 *                -1 if |X| is lesser  than |Y| or
 *                 0 if |X| is equal to |Y|
 */
int mpi_cmp_abs(mpi_t *X, mpi_t *Y);

/**
 * \brief          Compare signed values
 *
 * \return         1 if X is greater than Y,
 *                -1 if X is lesser  than Y or
 *                 0 if X is equal to Y
 */
int mpi_cmp_mpi(mpi_t *X, mpi_t *Y);

/**
 * \brief          Compare signed values
 *
 * \return         1 if X is greater than z,
 *                -1 if X is lesser  than z or
 *                 0 if X is equal to z
 */
int mpi_cmp_int(mpi_t *X, int z);

/**
 * \brief          Unsigned addition: X = |A| + |B|
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_add_abs(mpi_t *X, mpi_t *A, mpi_t *B);

/**
 * \brief          Unsigned substraction: X = |A| - |B|
 *
 * \return         0 if successful,
 *                 XYSSL_ERR_MPI_NEGATIVE_VALUE if B is greater than A
 */
int mpi_sub_abs(mpi_t *X, mpi_t *A, mpi_t *B);

/**
 * \brief          Signed addition: X = A + B
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_add_mpi(mpi_t *X, mpi_t *A, mpi_t *B);

/**
 * \brief          Signed substraction: X = A - B
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_sub_mpi(mpi_t *X, mpi_t *A, mpi_t *B);

/**
 * \brief          Signed addition: X = A + b
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_add_int(mpi_t *X, mpi_t *A, int b);

/**
 * \brief          Signed substraction: X = A - b
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_sub_int(mpi_t *X, mpi_t *A, int b);

/**
 * \brief          Baseline multiplication: X = A * B
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_mul_mpi(mpi_t *X, mpi_t *A, mpi_t *B);

/**
 * \brief          Baseline multiplication: X = A * b
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_mul_int(mpi_t *X, mpi_t *A, uint32_t b);

/**
 * \brief          Division by mpi_t: A = Q * B + R
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed,
 *                 XYSSL_ERR_MPI_DIVISION_BY_ZERO if B == 0
 *
 * \note           Either Q or R can be NULL.
 */
int mpi_div_mpi(mpi_t *Q, mpi_t *R, mpi_t *A, mpi_t *B);

/**
 * \brief          Division by int: A = Q * b + R
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed,
 *                 XYSSL_ERR_MPI_DIVISION_BY_ZERO if b == 0
 *
 * \note           Either Q or R can be NULL.
 */
int mpi_div_int(mpi_t *Q, mpi_t *R, mpi_t *A, int b);

/**
 * \brief          Modulo: R = A mod B
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed,
 *                 XYSSL_ERR_MPI_DIVISION_BY_ZERO if B == 0
 */
int mpi_mod_mpi(mpi_t *R, mpi_t *A, mpi_t *B);

/**
 * \brief          Modulo: r = A mod b
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed,
 *                 XYSSL_ERR_MPI_DIVISION_BY_ZERO if b == 0
 */
int mpi_mod_int(uint32_t *r, mpi_t *A, int b);

/**
 * \brief          Sliding-window exponentiation: X = A^E mod N
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed,
 *                 XYSSL_ERR_MPI_BAD_INPUT_DATA if N is negative or even
 *
 * \note           _RR is used to avoid re-computing R*R mod N across
 *                 multiple calls, which speeds up things a bit. It can
 *                 be set to NULL if the extra performance is unneeded.
 */
int mpi_exp_mod(mpi_t *X, mpi_t *A, mpi_t *E, mpi_t *N, mpi_t *_RR);

/**
 * \brief          Greatest common divisor: G = gcd(A, B)
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed
 */
int mpi_gcd(mpi_t *G, mpi_t *A, mpi_t *B);

/**
 * \brief          Modular inverse: X = A^-1 mod N
 *
 * \return         0 if successful,
 *                 1 if memory allocation failed,
 *                 XYSSL_ERR_MPI_BAD_INPUT_DATA if N is negative or nil
 *                 XYSSL_ERR_MPI_NOT_ACCEPTABLE if A has no inverse mod N
 */
int mpi_inv_mod(mpi_t *X, mpi_t *A, mpi_t *N);

/**
 * \brief          Miller-Rabin primality test
 *
 * \return         0 if successful (probably prime),
 *                 1 if memory allocation failed,
 *                 XYSSL_ERR_MPI_NOT_ACCEPTABLE if X is not prime
 */
int mpi_is_prime(mpi_t *X, int (*f_rng)(void *), void *p_rng);

/**
 * \brief          Prime number generation
 *
 * \param X        destination mpi_t
 * \param nbits    required size of X in bits
 * \param dh_flag  if 1, then (X-1)/2 will be prime too
 * \param f_rng    RNG function
 * \param p_rng    RNG parameter
 *
 * \return         0 if successful (probably prime),
 *                 1 if memory allocation failed,
 *                 XYSSL_ERR_MPI_BAD_INPUT_DATA if nbits is < 3
 */
int mpi_gen_prime(mpi_t *X, int nbits, int dh_flag,
                  int (*f_rng)(void *), void *p_rng);

/**
 * \brief          Checkup routine
 *
 * \return         0 if successful, or 1 if the test failed
 */
int mpi_self_test(int verbose);

#ifdef __cplusplus
}
#endif

#endif /* bignum.h */
