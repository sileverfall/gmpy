/* gmpy_mpmath.c
 *
 * Internal helper function for mpmath.
 */

static PyObject *
mpmath_build_mpf(long sign, PympzObject *man, PyObject *exp, long bc)
{
    PyObject *tup, *tsign, *tbc;
    if(!(tup = PyTuple_New(4))){
        Py_DECREF((PyObject*)man);
        Py_DECREF(exp);
        return NULL;
    }
    if(!(tsign=Py2or3Int_FromLong(sign))){
        Py_DECREF((PyObject*)man);
        Py_DECREF(exp);
        Py_DECREF(tup);
        return NULL;
    }
    if(!(tbc=Py2or3Int_FromLong(bc))){
        Py_DECREF((PyObject*)man);
        Py_DECREF(exp);
        Py_DECREF(tup);
        Py_DECREF(tsign);
        return NULL;
    }
    PyTuple_SET_ITEM(tup, 0, tsign);
    PyTuple_SET_ITEM(tup, 1, (PyObject*)man);
    PyTuple_SET_ITEM(tup, 2, (exp)?exp:Py2or3Int_FromLong(0));
    PyTuple_SET_ITEM(tup, 3, tbc);
    return tup;
}

static char doc_mpmath_normalizeg[]="\
_mpmath_normalize(...): helper function for mpmath.\n\
";
static PyObject *
Pympz_mpmath_normalize(PyObject *self, PyObject *args)
{
    long sign = 0, bc = 0, prec = 0, shift, zbits, carry = 0;
    PyObject *exp = 0, *newexp = 0, *newexp2 = 0, *tmp = 0;
    PympzObject *man = 0;
    mpz_t upper, lower;
    char rnd = 0;

    if(PyTuple_GET_SIZE(args) == 6){
        /* Need better error-checking here. Under Python 3.0, overflow into
           C-long is possible. */
        sign = Py2or3Int_AsLong(PyTuple_GET_ITEM(args, 0));
        man = (PympzObject *)PyTuple_GET_ITEM(args, 1);
        exp = PyTuple_GET_ITEM(args, 2);
        bc = Py2or3Int_AsLong(PyTuple_GET_ITEM(args, 3));
        prec = Py2or3Int_AsLong(PyTuple_GET_ITEM(args, 4));
        rnd = Py2or3String_AsString(PyTuple_GET_ITEM(args, 5))[0];
        if(PyErr_Occurred()){
            PyErr_SetString(PyExc_TypeError, "arguments long, PympzObject*,"
                "PyObject*, long, long, char needed");
            return NULL;
        }
    } else {
        PyErr_SetString(PyExc_TypeError, "6 arguments required");
        return NULL;
    }
    if(!Pympz_Check(man)){
        PyErr_SetString(PyExc_TypeError, "argument is not an mpz");
        return NULL;
    }

    /* If the mantissa is 0, return the normalized representation. */
    if(!mpz_sgn(man->z)) {
        Py_INCREF((PyObject*)man);
        return mpmath_build_mpf(0, man, 0, 0);
    }

    /* if bc <= prec and the number is odd return it */
    if ((bc <= prec) && mpz_odd_p(man->z)) {
        Py_INCREF((PyObject*)man);
        Py_INCREF((PyObject*)exp);
        return mpmath_build_mpf(sign, man, exp, bc);
    }

    mpz_inoc(upper);
    mpz_inoc(lower);

    shift = bc - prec;
    if(shift>0) {
        switch(rnd) {
            case 'f':
                if(sign) {
                    mpz_cdiv_q_2exp(upper, man->z, shift);
                } else {
                    mpz_fdiv_q_2exp(upper, man->z, shift);
                }
                break;
            case 'c':
                if(sign) {
                    mpz_fdiv_q_2exp(upper, man->z, shift);
                } else {
                    mpz_cdiv_q_2exp(upper, man->z, shift);
                }
                break;
            case 'd':
                mpz_fdiv_q_2exp(upper, man->z, shift);
                break;
            case 'u':
                mpz_cdiv_q_2exp(upper, man->z, shift);
                break;
            case 'n':
            default:
                mpz_tdiv_r_2exp(lower, man->z, shift);
                mpz_tdiv_q_2exp(upper, man->z, shift);
                if(mpz_sgn(lower)) {
                    /* lower is not 0 so it must have at least 1 bit set */
                    if(mpz_sizeinbase(lower, 2)==shift) {
                        /* lower is >= 1/2 */
                        if(mpz_scan1(lower, 0)==shift-1) {
                            /* lower is exactly 1/2 */
                            if(mpz_odd_p(upper))
                                carry = 1;
                        } else {
                            carry = 1;
                        }
                    }
                }
                if(carry)
                    mpz_add_ui(upper, upper, 1);
        }
        if (!(tmp = Py2or3Int_FromLong(shift))) {
            mpz_cloc(upper);
            mpz_cloc(lower);
            return NULL;
        }
        if (!(newexp = PyNumber_Add(exp, tmp))) {
            mpz_cloc(upper);
            mpz_cloc(lower);
            Py_DECREF(tmp);
            return NULL;
        }
        Py_DECREF(tmp);
        bc = prec;
    } else {
        mpz_set(upper, man->z);
        newexp = exp;
        Py_INCREF(newexp);
    }

    /* Strip trailing 0 bits. */
    if((zbits = mpz_scan1(upper, 0)))
        mpz_tdiv_q_2exp(upper, upper, zbits);

    if (!(tmp = Py2or3Int_FromLong(zbits))) {
        mpz_cloc(upper);
        mpz_cloc(lower);
        Py_DECREF(newexp);
        return NULL;
    }
    if (!(newexp2 = PyNumber_Add(newexp, tmp))) {
        mpz_cloc(upper);
        mpz_cloc(lower);
        Py_DECREF(tmp);
        Py_DECREF(newexp);
        return NULL;
    }
    Py_DECREF(newexp);
    Py_DECREF(tmp);

    bc -= zbits;
    /* Check if one less than a power of 2 was rounded up. */
    if(!mpz_cmp_ui(upper, 1))
        bc = 1;

    mpz_cloc(lower);
    return mpmath_build_mpf(sign, Pympz_FROM_MPZ(upper), newexp2, bc);
}

static char doc_mpmath_createg[]="\
_mpmath_create(...): helper function for mpmath.\n\
";
static PyObject *
Pympz_mpmath_create(PyObject *self, PyObject *args)
{
    long sign, bc, shift, zbits, carry = 0;
    PyObject *exp = 0, *newexp = 0, *newexp2 = 0, *tmp = 0, *precobj = 0;
    PympzObject *man = 0, *upper = 0, *lower = 0;

    char rnd = 'f';
    long prec = 0;

#if PY_MAJOR_VERSION >= 3
    if(!PyArg_ParseTuple(args, "O&O|OC", Pympz_convert_arg, &man, &exp, &precobj, &rnd))
#else
    if(!PyArg_ParseTuple(args, "O&O|Oc", Pympz_convert_arg, &man, &exp, &precobj, &rnd))
#endif
        return NULL;
    assert(Pympz_Check(man));

    /* If the mantissa is 0, return the normalized representation. */
    if(!mpz_sgn(man->z)) {
        return mpmath_build_mpf(0, man, 0, 0);
    }

    upper = Pympz_new();
    lower = Pympz_new();
    if(!upper||!lower) {
        Py_DECREF((PyObject*)man);
        Py_XDECREF((PyObject*)upper);
        Py_XDECREF((PyObject*)lower);
        return NULL;
    }

    /* Extract sign, make man positive, and set bit count */
    sign = (mpz_sgn(man->z) == -1);
    mpz_abs(upper->z, man->z);
    bc = mpz_sizeinbase(upper->z, 2);

    /* Check desired precision */
    if((precobj)&&(Py2or3Int_Check(precobj))) prec = abs(Py2or3Int_AsLong(precobj));
    if(!prec) prec = bc;

    shift = bc - prec;
    if(shift>0) {
        switch(rnd) {
            case 'f':
                if(sign) {
                    mpz_cdiv_q_2exp(upper->z, upper->z, shift);
                } else {
                    mpz_fdiv_q_2exp(upper->z, upper->z, shift);
                }
                break;
            case 'c':
                if(sign) {
                    mpz_fdiv_q_2exp(upper->z, upper->z, shift);
                } else {
                    mpz_cdiv_q_2exp(upper->z, upper->z, shift);
                }
                break;
            case 'd':
                mpz_fdiv_q_2exp(upper->z, upper->z, shift);
                break;
            case 'u':
                mpz_cdiv_q_2exp(upper->z, upper->z, shift);
                break;
            case 'n':
            default:
                mpz_tdiv_r_2exp(lower->z, upper->z, shift);
                mpz_tdiv_q_2exp(upper->z, upper->z, shift);
                if(mpz_sgn(lower->z)) {
                    /* lower is not 0 so it must have at least 1 bit set */
                    if(mpz_sizeinbase(lower->z, 2)==shift) {
                        /* lower is >= 1/2 */
                        if(mpz_scan1(lower->z, 0)==shift-1) {
                            /* lower is exactly 1/2 */
                            if(mpz_odd_p(upper->z))
                                carry = 1;
                        } else {
                            carry = 1;
                        }
                    }
                }
                if(carry)
                    mpz_add_ui(upper->z, upper->z, 1);
        }
        if (!(tmp = Py2or3Int_FromLong(shift))) {
            Py_DECREF((PyObject*)upper);
            Py_DECREF((PyObject*)lower);
            return NULL;
        }
        if (!(newexp = PyNumber_Add(exp, tmp))) {
            Py_DECREF((PyObject*)man);
            Py_DECREF((PyObject*)upper);
            Py_DECREF((PyObject*)lower);
            Py_DECREF(tmp);
            return NULL;
        }
        Py_DECREF(tmp);
        bc = prec;
    } else {
        newexp = exp;
        Py_INCREF(newexp);
    }

    /* Strip trailing 0 bits. */
    if((zbits = mpz_scan1(upper->z, 0)))
        mpz_tdiv_q_2exp(upper->z, upper->z, zbits);

    if (!(tmp = Py2or3Int_FromLong(zbits))) {
        Py_DECREF((PyObject*)man);
        Py_DECREF((PyObject*)upper);
        Py_DECREF((PyObject*)lower);
        Py_DECREF(newexp);
        return NULL;
    }
    if (!(newexp2 = PyNumber_Add(newexp, tmp))) {
        Py_DECREF((PyObject*)man);
        Py_DECREF((PyObject*)upper);
        Py_DECREF((PyObject*)lower);
        Py_DECREF(tmp);
        Py_DECREF(newexp);
        return NULL;
    }
    Py_DECREF(newexp);
    Py_DECREF(tmp);

    bc -= zbits;
    /* Check if one less than a power of 2 was rounded up. */
    if(!mpz_cmp_ui(upper->z, 1))
        bc = 1;

    Py_DECREF((PyObject*)lower);
    Py_DECREF((PyObject*)man);
    return mpmath_build_mpf(sign, upper, newexp2, bc);
}

/* Second version of helper functions for mpmath. See Issue 33 for details.
 */

static PyObject *
do_mpmath_trim(mpz_t xman, mpz_t xexp, long prec, char rnd) {
    PyObject *result = 0;
    PympzObject *rman = 0, *rexp = 0;
    long bc, shift, zbits, carry = 0;
    mpz_t lower;

    result = PyTuple_New(2);
    rman = Pympz_new();
    rexp = Pympz_new();
    if(!result || !rman || !rexp) {
        Py_XDECREF(result);
        Py_XDECREF((PyObject*)rman);
        Py_XDECREF((PyObject*)rexp);
        return NULL;
    }
    mpz_set(rman->z, xman);
    mpz_set(rexp->z, xexp);

    /* If the mantissa is 0, just return the canonical representation of 0. */
    if(!mpz_sgn(rman->z)) {
        fprintf(stderr, "zero\n");
        mpz_set_ui(rexp->z, 0);
        goto return_result;
    }

    /* Remove trailing 0 bits and adjust exponenet. */
    if((zbits = mpz_scan1(rman->z, 0))) {
        mpz_tdiv_q_2exp(rman->z, rman->z, zbits);
        mpz_add_ui(rexp->z, rexp->z, zbits);
    }

    /* If prec is 0, return with trailing 0 bits removed. */
    if(prec == 0) goto return_result;

    bc = mpz_sizeinbase(rman->z, 2);

    /* If bc <= prec, just return. */
    if(bc <= prec) goto return_result;

    /* We need to round the mantissa. */
    shift = bc - prec;
    switch(rnd) {
        case 'f':
            mpz_fdiv_q_2exp(rman->z, rman->z, shift);
            break;
        case 'c':
            mpz_cdiv_q_2exp(rman->z, rman->z, shift);
            break;
        case 'd':
            if(mpz_sgn(rman->z) > 0) {
                mpz_fdiv_q_2exp(rman->z, rman->z, shift);
            } else {
                mpz_cdiv_q_2exp(rman->z, rman->z, shift);
            }
            break;
        case 'u':
            if(mpz_sgn(rman->z) > 0) {
                mpz_cdiv_q_2exp(rman->z, rman->z, shift);
            } else {
                mpz_fdiv_q_2exp(rman->z, rman->z, shift);
            }
            break;
        case 'n':
        default:
            mpz_inoc(lower);
            mpz_tdiv_r_2exp(lower, rman->z, shift);
            mpz_tdiv_q_2exp(rman->z, rman->z, shift);
            /* lower is not 0 so it must have at least 1 bit set */
            if(mpz_sizeinbase(lower, 2) == shift) {
                /* lower is >= 1/2 */
                if(mpz_scan1(lower, 0) == shift-1) {
                    /* lower is exactly 1/2 */
                    if(mpz_odd_p(rman->z))
                        carry = 1;
                } else {
                    carry = 1;
                }
            }
            mpz_cloc(lower);
            /* Add the carry bit and readjust for trailing 0 bits. */
            if(carry) {
                mpz_add_ui(rman->z, rman->z, 1);
                zbits = mpz_scan1(rman->z, 0);
                mpz_tdiv_q_2exp(rman->z, rman->z, zbits);
                mpz_add_ui(rexp->z, rexp->z, zbits);
            }
    }
    mpz_add_ui(rexp->z, rexp->z, shift);

return_result:
    PyTuple_SET_ITEM(result, 0, (PyObject*)rman);
    PyTuple_SET_ITEM(result, 1, (PyObject*)rexp);
    return result;
}

static char doc_mpmath_trimg[]="\
_mpmath_trim(xman, xexp, prec, rounding):\n\
    Return (man, exp) by rounding xman*(2**xexp) to prec bits using the\n\
    specified rounding mode.\n\
";
static PyObject *
Pympz_mpmath_trim(PyObject *self, PyObject *args)
{
    PyObject *arg0 = 0, *arg1 = 0, *result;
    long prec = 0;
    const char *rnd = "d";

    switch(PyTuple_GET_SIZE(args)) {
        case 4:
            rnd = Py2or3String_AsString(PyTuple_GET_ITEM(args, 3));
        case 3:
            prec = Py2or3Int_AsLong(PyTuple_GET_ITEM(args, 2));
        case 2:
            arg1 = (PyObject*)anyint2Pympz(PyTuple_GET_ITEM(args, 1));
        case 1:
            arg0 = (PyObject*)anyint2Pympz(PyTuple_GET_ITEM(args, 0));
    }
    if(!arg0 || !arg1 || PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "arguments mpz, mpz, long, char needed");
        Py_XDECREF(arg0);
        Py_XDECREF(arg1);
        return NULL;
    } else {
        result = do_mpmath_trim(Pympz_AS_MPZ(arg0), Pympz_AS_MPZ(arg1), prec, rnd[0]);
        Py_DECREF(arg0);
        Py_DECREF(arg1);
        return result;
    }
}

static char doc_mpmath_addg[]="\
_mpmath_add(xman, xexp, yman, yexp, prec, rounding):\n\
    Return (man, exp) by rounding xman*2**xexp + yman*2**yexp to prec\n\
    bits using the specified rounding mode.\n\
";
static PyObject *
Pympz_mpmath_add(PyObject *self, PyObject *args)
{
    return Py2or3Int_FromLong(0);
}

static char doc_mpmath_multg[]="\
_mpmath_mult(xman, xexp, yman, yexp, prec, rounding):\n\
    Return (man, exp) by rounding xman*2**xexp * yman*2**yexp to prec\n\
    bits using the specified rounding mode.\n\
";
static PyObject *
Pympz_mpmath_mult(PyObject *self, PyObject *args)
{
    PyObject *arg0 = 0, *arg1 = 0, *arg2 = 0, * arg3 = 0, *result;
    mpz_t man, exp;
    long prec = 0;
    const char *rnd = "d";

    switch(PyTuple_GET_SIZE(args)) {
        case 6:
            rnd = Py2or3String_AsString(PyTuple_GET_ITEM(args, 5));
        case 5:
            prec = Py2or3Int_AsLong(PyTuple_GET_ITEM(args, 4));
        case 4:
            arg3 = (PyObject*)anyint2Pympz(PyTuple_GET_ITEM(args, 3));
        case 3:
            arg2 = (PyObject*)anyint2Pympz(PyTuple_GET_ITEM(args, 2));
        case 2:
            arg1 = (PyObject*)anyint2Pympz(PyTuple_GET_ITEM(args, 1));
        case 1:
            arg0 = (PyObject*)anyint2Pympz(PyTuple_GET_ITEM(args, 0));
    }
    if(!arg0 || !arg1 || !arg2 || !arg3 || PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "arguments mpz, mpz, mpz, mpz, long, char needed");
        Py_XDECREF(arg0);
        Py_XDECREF(arg1);
        Py_XDECREF(arg2);
        Py_XDECREF(arg3);
        return NULL;
    } else {
        mpz_inoc(man);
        mpz_inoc(exp);
        mpz_mul(man, Pympz_AS_MPZ(arg0), Pympz_AS_MPZ(arg2));
        mpz_add(exp, Pympz_AS_MPZ(arg1), Pympz_AS_MPZ(arg3));
        result = do_mpmath_trim(man, exp, prec, rnd[0]);
        mpz_cloc(man);
        mpz_cloc(exp);
        Py_DECREF(arg0);
        Py_DECREF(arg1);
        Py_DECREF(arg2);
        Py_DECREF(arg3);
        return result;
    }
}

static char doc_mpmath_divg[]="\
_mpmath_div(xman, xexp, yman, yexp, prec, rounding):\n\
    Return (man, exp) by rounding xman*2**xexp / yman*2**yexp to prec\n\
    bits using the specified rounding mode.\n\
";
static PyObject *
Pympz_mpmath_div(PyObject *self, PyObject *args)
{
    return Py2or3Int_FromLong(0);
}

static char doc_mpmath_sqrtg[]="\
_mpmath_sqrt(man, exp, prec, rounding):\n\
    Return (man, exp) by rounding square_root(xman*2**xexp)) to prec\n\
    bits using the specified rounding mode.\n\
";
static PyObject *
Pympz_mpmath_sqrt(PyObject *self, PyObject *args)
{
    return Py2or3Int_FromLong(0);
}
