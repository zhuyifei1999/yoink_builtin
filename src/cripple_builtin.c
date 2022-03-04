#define PY_SSIZE_T_CLEAN
#include <stdbool.h>
#include <stddef.h>
#include <Python.h>

static PyThread_type_lock cripple_lock;

static PyThread_type_lock lockdown_lock;
static PyFrameObject *lockdown_frame;

struct crippled_info {
    void **addr;
    void *orig_func;
    void *new_func;
};

static PyObject *
crippled_pyobject()
{
    PyErr_SetString(PyExc_NotImplementedError,
                    "function has been crippled");
    return NULL;
}

static int
crippled_int()
{
    PyErr_SetString(PyExc_NotImplementedError,
                    "function has been crippled");
    return -1;
}

struct type_slot {
    const char *name;
    uintptr_t offset1;
    bool should_offset2;
    uintptr_t offset2;
    void *crippled_func;
};

#define generic_crippled(ptr) (_Generic((ptr), \
        int (*)(): crippled_int,          \
        long (*)(): crippled_int,          \
        PyObject *(*)(): crippled_pyobject    \
    ))

#define SLOT1(name) {                                  \
        #name,                                         \
        offsetof(PyTypeObject, name),                  \
        false,                                         \
        0,                                             \
        generic_crippled(((PyTypeObject *)NULL)->name) \
    }
#define SLOT2(indir, name) {#name,                              \
        offsetof(PyTypeObject, indir),                          \
        true,                                                   \
        offsetof(typeof(*((PyTypeObject *)NULL)->indir), name), \
        generic_crippled(((PyTypeObject *)NULL)->indir->name)  \
    }

static struct type_slot type_slots[] = {
    SLOT1(tp_getattr),
    SLOT1(tp_setattr),
    SLOT1(tp_repr),
    SLOT1(tp_hash),
    SLOT1(tp_call),
    SLOT1(tp_str),
    SLOT1(tp_getattro),
    SLOT1(tp_setattro),
    SLOT1(tp_richcompare),
    SLOT1(tp_iter),
    SLOT1(tp_iternext),
    SLOT1(tp_descr_get),
    SLOT1(tp_descr_set),
    SLOT1(tp_init),
    SLOT1(tp_alloc),
    SLOT1(tp_new),
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 8
    SLOT1(tp_vectorcall),
#endif

    SLOT2(tp_as_number, nb_add),
    SLOT2(tp_as_number, nb_subtract),
    SLOT2(tp_as_number, nb_multiply),
    SLOT2(tp_as_number, nb_remainder),
    SLOT2(tp_as_number, nb_divmod),
    SLOT2(tp_as_number, nb_power),
    SLOT2(tp_as_number, nb_negative),
    SLOT2(tp_as_number, nb_positive),
    SLOT2(tp_as_number, nb_absolute),
    SLOT2(tp_as_number, nb_bool),
    SLOT2(tp_as_number, nb_invert),
    SLOT2(tp_as_number, nb_lshift),
    SLOT2(tp_as_number, nb_rshift),
    SLOT2(tp_as_number, nb_and),
    SLOT2(tp_as_number, nb_xor),
    SLOT2(tp_as_number, nb_or),
    SLOT2(tp_as_number, nb_int),
    // SLOT2(tp_as_number, nb_reserved),
    SLOT2(tp_as_number, nb_float),
    SLOT2(tp_as_number, nb_inplace_add),
    SLOT2(tp_as_number, nb_inplace_subtract),
    SLOT2(tp_as_number, nb_inplace_multiply),
    SLOT2(tp_as_number, nb_inplace_remainder),
    SLOT2(tp_as_number, nb_inplace_power),
    SLOT2(tp_as_number, nb_inplace_lshift),
    SLOT2(tp_as_number, nb_inplace_rshift),
    SLOT2(tp_as_number, nb_inplace_and),
    SLOT2(tp_as_number, nb_inplace_xor),
    SLOT2(tp_as_number, nb_inplace_or),
    SLOT2(tp_as_number, nb_floor_divide),
    SLOT2(tp_as_number, nb_true_divide),
    SLOT2(tp_as_number, nb_inplace_floor_divide),
    SLOT2(tp_as_number, nb_inplace_true_divide),
    SLOT2(tp_as_number, nb_index),
    SLOT2(tp_as_number, nb_matrix_multiply),
    SLOT2(tp_as_number, nb_inplace_matrix_multiply),

    SLOT2(tp_as_sequence, sq_length),
    SLOT2(tp_as_sequence, sq_concat),
    SLOT2(tp_as_sequence, sq_repeat),
    SLOT2(tp_as_sequence, sq_item),
    // SLOT2(tp_as_sequence, was_sq_slice),
    SLOT2(tp_as_sequence, sq_ass_item),
    // SLOT2(tp_as_sequence, was_sq_ass_slice),
    SLOT2(tp_as_sequence, sq_contains),
    SLOT2(tp_as_sequence, sq_inplace_concat),
    SLOT2(tp_as_sequence, sq_inplace_repeat),

    SLOT2(tp_as_mapping, mp_length),
    SLOT2(tp_as_mapping, mp_subscript),
    SLOT2(tp_as_mapping, mp_ass_subscript),

    SLOT2(tp_as_async, am_await),
    SLOT2(tp_as_async, am_aiter),
    SLOT2(tp_as_async, am_anext),
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 10
    SLOT2(tp_as_async, am_send),
#endif

    SLOT2(tp_as_buffer, bf_getbuffer),
    // SLOT2(tp_as_buffer, bf_releasebuffer),
};

static void
info_destruct(PyObject *capsule)
{
    struct crippled_info *info = PyCapsule_GetPointer(capsule,
        "crippled_builtin.uncripple_info");
    PyMem_RawFree(info);
}

static PyObject *
do_cripple(void **addr, void *new_func)
{
    struct crippled_info *info;
    PyObject *capsule;

    PyThread_acquire_lock(lockdown_lock, 1);
    if (lockdown_frame) {
        PyThread_release_lock(lockdown_lock);
        PyErr_SetString(PyExc_RuntimeError, "lockdown");
        return NULL;
    }
    PyThread_release_lock(lockdown_lock);

    info = PyMem_RawMalloc(sizeof(*info));
    if (!info)
        return PyErr_NoMemory();

    capsule = PyCapsule_New(info, "crippled_builtin.uncripple_info", info_destruct);
    if (!capsule) {
        PyMem_RawFree(info);
        return NULL;
    }

    PyThread_acquire_lock(cripple_lock, 1);
    info->addr = addr;
    info->orig_func = *addr;
    info->new_func = new_func;

    *addr = new_func;
    PyThread_release_lock(cripple_lock);

    return capsule;
}

static PyObject *
cripple_function(PyObject *self, PyObject *args)
{
    PyObject *func, *ret;

    if (!PyArg_ParseTuple(args, "O!", &PyCFunction_Type, &func))
        return NULL;

    return do_cripple((void **)&PyCFunction_GET_FUNCTION(func), crippled_pyobject);
}

static PyObject *
cripple_type_slot(PyObject *self, PyObject *args)
{
    PyObject *type, *ret;
    const char *name;

    if (!PyArg_ParseTuple(args, "O!s", &PyType_Type, &type, &name))
        return NULL;

    for (struct type_slot *slot = type_slots; slot->name; slot++) {
        if (!strcmp(slot->name, name)) {
            void **ptr = (void *)type + slot->offset1;
            if (slot->should_offset2)
                ptr = *ptr + slot->offset2;

            return do_cripple(ptr, slot->crippled_func);
        }
    }

    PyErr_SetString(PyExc_NotImplementedError, "no such slot");
    return NULL;
}

static PyObject *
uncripple(PyObject *self, PyObject *args)
{
    PyObject *capsule;
    bool correct;

    if (!PyArg_ParseTuple(args, "O!", &PyCapsule_Type, &capsule))
        return NULL;

    PyThread_acquire_lock(lockdown_lock, 1);
    if (lockdown_frame) {
        PyThread_release_lock(lockdown_lock);
        PyErr_SetString(PyExc_RuntimeError, "lockdown");
        return NULL;
    }
    PyThread_release_lock(lockdown_lock);

    struct crippled_info *info = PyCapsule_GetPointer(capsule,
        "crippled_builtin.uncripple_info");
    if (!info)
        return NULL;

    PyThread_acquire_lock(cripple_lock, 1);
    if ((correct = *info->addr == info->new_func))
        *info->addr = info->orig_func;
    PyThread_release_lock(cripple_lock);

    if (!correct) {
        PyErr_SetString(PyExc_AssertionError, "function pointer was modified?!");
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
lockdown(PyObject *self, PyObject *args)
{
    PyThread_acquire_lock(lockdown_lock, 1);
    if (lockdown_frame) {
        PyThread_release_lock(lockdown_lock);
        PyErr_SetString(PyExc_RuntimeError, "lockdown");
        return NULL;
    }

    lockdown_frame = PyEval_GetFrame();
    Py_XINCREF(lockdown_frame);
    PyThread_release_lock(lockdown_lock);

    Py_RETURN_NONE;
}

static PyObject *
unlockdown(PyObject *self, PyObject *args)
{
    PyThread_acquire_lock(lockdown_lock, 1);
    if (!lockdown_frame) {
        PyThread_release_lock(lockdown_lock);
        PyErr_SetString(PyExc_RuntimeError, "no lockdown");
        return NULL;
    }

    if (lockdown_frame != PyEval_GetFrame()) {
        PyThread_release_lock(lockdown_lock);
        PyErr_SetString(PyExc_RuntimeError, "bad frame; call unlockdown from same frame as lockdown");
        return NULL;
    }

    Py_XDECREF(lockdown_frame);
    lockdown_frame = NULL;
    PyThread_release_lock(lockdown_lock);

    Py_RETURN_NONE;
}

static PyMethodDef cripple_builtin_methods[] = {
    {"cripple_function", cripple_function, METH_VARARGS, ""},
    {"cripple_type_slot", cripple_type_slot, METH_VARARGS, ""},

    {"uncripple", uncripple, METH_VARARGS, ""},

    {"lockdown", lockdown, METH_NOARGS, ""},
    {"unlockdown", unlockdown, METH_NOARGS, ""},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef cripple_builtin_module = {
    PyModuleDef_HEAD_INIT,
    "cripple_builtin",
    "",
    -1,
    cripple_builtin_methods
};

PyMODINIT_FUNC
PyInit_cripple_builtin(void)
{
    cripple_lock = PyThread_allocate_lock();
    if (!cripple_lock) {
        PyErr_SetString(PyExc_RuntimeError, "lock creation failed");
        return NULL;
    }

    lockdown_lock = PyThread_allocate_lock();
    if (!lockdown_lock) {
        PyErr_SetString(PyExc_RuntimeError, "lock creation failed");
        return NULL;
    }

    return PyModule_Create(&cripple_builtin_module);
}
