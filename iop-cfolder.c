/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "iop.h"

#define SIGNED(expr)    ((int64_t)(expr))

static int cf_get_last_type(qv_t(cf_elem) *stack)
{
    if (stack->len > 0)
        return stack->tab[stack->len - 1].type;
    return CF_ELEM_STACK_EMPTY;
}
static iop_cfolder_elem_t *cf_get_prev_op(qv_t(cf_elem) *stack)
{
    int pos = stack->len - 1;

    while (pos >= 0) {
        if (stack->tab[pos].type == CF_ELEM_OP)
            return &stack->tab[pos];
        pos--;
    }
    return NULL;
}
static int cf_op_precedence(iop_cfolder_op_t op, bool unary)
{
    /* From highest precedence to lowest */
    switch (op) {
      case CF_OP_EXP:
        return 7;
      case CF_OP_NOT:
        return 6;
      case CF_OP_MUL:
      case CF_OP_DIV:
      case CF_OP_MOD:
        return 5;
      case CF_OP_ADD:
        return 4;
      case CF_OP_SUB:
        if (unary) {
            return 6;
        } else {
            return 4;
        }
      case CF_OP_LSHIFT:
      case CF_OP_RSHIFT:
        return 3;
      case CF_OP_AND:
        return 2;
      case CF_OP_OR:
      case CF_OP_XOR:
        return 1;
      default:
        return -1;
    }
}

/* Return true if an operator has a right associativity */
static bool cf_op_is_rassoc(iop_cfolder_op_t op, bool unary)
{
    return op == CF_OP_NOT || op == CF_OP_EXP || (unary && op == CF_OP_SUB);
}

/* Stack abstraction */
static void cf_stack_push(qv_t(cf_elem) *stack, iop_cfolder_elem_t elem)
{
    qv_append(cf_elem, stack, elem);
}
static int cf_stack_pop(qv_t(cf_elem) *stack, iop_cfolder_elem_t *elem)
{
    if (stack->len <= 0)
        return -1;
    if (elem)
        *elem = stack->tab[stack->len - 1];
    qv_remove(cf_elem, stack, stack->len - 1);
    return 0;
}

static int cf_reduce(qv_t(cf_elem) *stack)
{
    iop_cfolder_elem_t eleft, op, eright, res;
    bool unary = false;
    cf_elem_init(&res);

    PS_WANT(stack->len >= 2);
    cf_stack_pop(stack, &eright);
    PS_WANT(eright.type == CF_ELEM_NUMBER);
    cf_stack_pop(stack, &op);
    PS_WANT(op.type == CF_ELEM_OP);

    if (op.op == CF_OP_LPAREN || op.op == CF_OP_RPAREN)
        return -1;

    if (op.unary) {
        unary = true;
    } else {
        PS_CHECK(cf_stack_pop(stack, &eleft));
        PS_WANT(eleft.type == CF_ELEM_NUMBER);
    }

    /* Compute eleft OP eright */
    switch (op.op) {
        /* Arithmetic operations */
      case CF_OP_ADD:
        res.num       = eleft.num + eright.num;
        res.is_signed = eleft.is_signed || eright.is_signed;
        break;
      case CF_OP_SUB:
        if (unary) {
            res.num       = -eright.num;
            res.is_signed = !eright.is_signed;
        } else {
            res.num = eleft.num - eright.num;
            if (eleft.is_signed && eright.is_signed) {
                res.is_signed = SIGNED(eleft.num) > SIGNED(eright.num);
            } else
            if (eleft.is_signed && !eright.is_signed) {
                res.is_signed = true;
            } else
            if (!eleft.is_signed && eright.is_signed) {
                res.is_signed = false;
            } else {
                res.is_signed = eleft.num < eright.num;
            }
        }
        break;
      case CF_OP_MUL:
        res.is_signed = eleft.is_signed ^ eright.is_signed;
        if (eleft.is_signed || eright.is_signed) {
            res.num = SIGNED(eleft.num) * SIGNED(eright.num);
        } else {
            res.num = eleft.num * eright.num;
        }
        break;
      case CF_OP_DIV:
        res.is_signed = eleft.is_signed ^ eright.is_signed;
        if (eleft.is_signed || eright.is_signed) {
            res.num = SIGNED(eleft.num) / SIGNED(eright.num);
        } else {
            res.num = eleft.num / eright.num;
        }
        break;
      case CF_OP_MOD:
        res.is_signed = eleft.is_signed ^ eright.is_signed;
        if (eleft.is_signed || eright.is_signed) {
            res.num = SIGNED(eleft.num) % SIGNED(eright.num);
        } else {
            res.num = eleft.num % eright.num;
        }
        break;
      case CF_OP_EXP:
        /* Negative exponent are forbidden */
        if (eright.is_signed)
            return e_error("negative expressions are forbidden when used as"
                           " exponent.");

        res.num = 1;
        res.is_signed = eleft.is_signed && (eright.num % 2 != 0);
        if (eleft.is_signed) {
            while (eright.num-- > 0) {
                res.num *= SIGNED(eleft.num);
            }
        } else {
            while (eright.num-- > 0) {
                res.num *= eleft.num;
            }
        }
        break;

        /* Logical operations */
        /* XXX when a logical expression is used, the result is considered as
         * an unsigned expression */
      case CF_OP_XOR:
        res.num = eleft.num ^ eright.num;
        break;
      case CF_OP_AND:
        res.num = eleft.num & eright.num;
        break;
      case CF_OP_OR:
        res.num = eleft.num | eright.num;
        break;
      case CF_OP_NOT:
        res.num = ~eright.num;
        break;
      case CF_OP_LSHIFT:
        res.num = eleft.num << eright.num;
        break;
      case CF_OP_RSHIFT:
        res.num = eleft.num >> eright.num;
        break;
      default:
        return e_error("unknown operator");
    }

    res.type = CF_ELEM_NUMBER;
    cf_stack_push(stack, res);
    return 0;
}

static int cf_reduce_all(qv_t(cf_elem) *stack)
{
    while (stack->len > 1) {
        PS_CHECK(cf_reduce(stack));
    }
    return 0;
}

static int cf_reduce_until_paren(qv_t(cf_elem) *stack)
{
    iop_cfolder_elem_t num, op;

    while (stack->len > 1) {
        iop_cfolder_elem_t *sptr = &stack->tab[stack->len - 2];

        if (sptr->type == CF_ELEM_OP && sptr->op == CF_OP_LPAREN)
            break;

        PS_CHECK(cf_reduce(stack));
    }

    /* Pop the reduced number and the open parentheses */
    PS_CHECK(cf_stack_pop(stack, &num));
    PS_WANT(num.type == CF_ELEM_NUMBER);

    PS_CHECK(cf_stack_pop(stack, &op));
    PS_WANT(op.type == CF_ELEM_OP);
    PS_WANT(op.op   == CF_OP_LPAREN);

    /* Replace the number */
    cf_stack_push(stack, num);

    return 0;
}

int iop_cfolder_feed_number(iop_cfolder_t *folder, uint64_t num, bool is_signed)
{
    iop_cfolder_elem_t elem;
    cf_elem_init(&elem);

    if (cf_get_last_type(&folder->stack) == CF_ELEM_NUMBER)
        return e_error("there is already a number on the stack");

    elem.type      = CF_ELEM_NUMBER;
    elem.num       = num;
    elem.is_signed = is_signed && SIGNED(num) < 0;

    cf_stack_push(&folder->stack, elem);
    return 0;
}

int iop_cfolder_feed_operator(iop_cfolder_t *folder, iop_cfolder_op_t op)
{
    iop_cfolder_elem_t elem;
    int op_prec;

    cf_elem_init(&elem);
    elem.type  = CF_ELEM_OP;
    elem.num   = op;

    if (cf_get_last_type(&folder->stack) != CF_ELEM_NUMBER) {
        /* Check for an unary operator */
        switch (op) {
          case CF_OP_SUB:
          case CF_OP_NOT:
            elem.type  = CF_ELEM_OP;
            elem.op    = op;
            elem.unary = true;
            goto shift;
          case CF_OP_LPAREN:
            folder->paren_cnt++;
            goto shift;
          default:
            return e_error("an unary operator was expected");
        }
    }

    /* Number case */
    if (op == CF_OP_NOT || op == CF_OP_LPAREN)
        return e_error("a binary operator was expected");

    /* Handle parentheses */
    if (op == CF_OP_RPAREN) {
        folder->paren_cnt--;
        if (folder->paren_cnt < 0)
            return e_error("there is too many closed parentheses");
        /* Reduce until we reach an open parentheses */
        if (cf_reduce_until_paren(&folder->stack) < 0)
            return e_error("invalid closed perentheses position");
        return 0;
    }

    PS_CHECK(op_prec = cf_op_precedence(op, false));

    /* Test for reduce */
    for (iop_cfolder_elem_t *pelem; (pelem = cf_get_prev_op(&folder->stack)); ) {
        iop_cfolder_op_t pop = pelem->op;
        int pop_prec         = cf_op_precedence(pop, pelem->unary);

        if (pop_prec > op_prec) {
            /* The previous operator has a higest priority than the new
             * one, we reduce it before to continue and we check again */
            PS_CHECK(cf_reduce(&folder->stack));
            continue;
        } else
        if (pop_prec == op_prec) {
            /* If precendences are equal, then a right associative operator
             * continue to shift whereas a left associative operator reduce */
            if (!cf_op_is_rassoc(pop, pelem->unary)) {
                PS_CHECK(cf_reduce(&folder->stack));
            }
        }

        /* If the previous operator has a lowest priority than the new one we
         * continue to shift */
        break;
    }

    /* Now shift the new operator */
  shift:
    cf_stack_push(&folder->stack, elem);

    return 0;
}

int iop_cfolder_get_result(iop_cfolder_t *folder, uint64_t *res)
{
    iop_cfolder_elem_t elem;
    if (folder->stack.len == 0)
        return e_error("there is nothing on the stack");

    if (folder->paren_cnt)
        return e_error("there too many opened parentheses");

    /* Reduce until the end */
    if (cf_reduce_all(&folder->stack) < 0)
        return e_error("can't reduce completly the stack");

    cf_stack_pop(&folder->stack, &elem);
    if (elem.type != CF_ELEM_NUMBER)
        return e_error("invalid stack content");

    *res = elem.num;

    return 0;
}
