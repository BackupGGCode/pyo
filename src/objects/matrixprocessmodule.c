/*************************************************************************
 * Copyright 2010 Olivier Belanger                                        *                  
 *                                                                        * 
 * This file is part of pyo, a python module to help digital signal       *
 * processing script creation.                                            *  
 *                                                                        * 
 * pyo is free software: you can redistribute it and/or modify            *
 * it under the terms of the GNU General Public License as published by   *
 * the Free Software Foundation, either version 3 of the License, or      *
 * (at your option) any later version.                                    * 
 *                                                                        *
 * pyo is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *    
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with pyo.  If not, see <http://www.gnu.org/licenses/>.           *
 *************************************************************************/

#include <Python.h>
#include "structmember.h"
#include <math.h>
#include "pyomodule.h"
#include "streammodule.h"
#include "servermodule.h"
#include "dummymodule.h"
#include "matrixmodule.h"
#include "interpolation.h"

static float
_clip(float x) {
    if (x < 0.0)
        return 0.0;
    else if (x >= 1.0)
        return 1.0;
    else
        return x;
}

/**************/
/* MatrixPointer object */
/**************/
typedef struct {
    pyo_audio_HEAD
    PyObject *matrix;
    PyObject *indexrow;
    Stream *indexrow_stream;
    PyObject *indexcol;
    Stream *indexcol_stream;
    int modebuffer[2];
} MatrixPointer;

static void
MatrixPointer_readframes(MatrixPointer *self) {
    int i;
    
    float *row = Stream_getData((Stream *)self->indexrow_stream);
    float *col = Stream_getData((Stream *)self->indexcol_stream);
    
    for (i=0; i<self->bufsize; i++) {
        self->data[i] = MatrixStream_getInterpPointFromPos(self->matrix, row[i], col[i]);
    }
}

static void MatrixPointer_postprocessing_ii(MatrixPointer *self) { POST_PROCESSING_II };
static void MatrixPointer_postprocessing_ai(MatrixPointer *self) { POST_PROCESSING_AI };
static void MatrixPointer_postprocessing_ia(MatrixPointer *self) { POST_PROCESSING_IA };
static void MatrixPointer_postprocessing_aa(MatrixPointer *self) { POST_PROCESSING_AA };
static void MatrixPointer_postprocessing_ireva(MatrixPointer *self) { POST_PROCESSING_IREVA };
static void MatrixPointer_postprocessing_areva(MatrixPointer *self) { POST_PROCESSING_AREVA };
static void MatrixPointer_postprocessing_revai(MatrixPointer *self) { POST_PROCESSING_REVAI };
static void MatrixPointer_postprocessing_revaa(MatrixPointer *self) { POST_PROCESSING_REVAA };
static void MatrixPointer_postprocessing_revareva(MatrixPointer *self) { POST_PROCESSING_REVAREVA };

static void
MatrixPointer_setProcMode(MatrixPointer *self)
{
    int muladdmode;
    muladdmode = self->modebuffer[0] + self->modebuffer[1] * 10;

    self->proc_func_ptr = MatrixPointer_readframes;

	switch (muladdmode) {
        case 0:        
            self->muladd_func_ptr = MatrixPointer_postprocessing_ii;
            break;
        case 1:    
            self->muladd_func_ptr = MatrixPointer_postprocessing_ai;
            break;
        case 2:    
            self->muladd_func_ptr = MatrixPointer_postprocessing_revai;
            break;
        case 10:        
            self->muladd_func_ptr = MatrixPointer_postprocessing_ia;
            break;
        case 11:    
            self->muladd_func_ptr = MatrixPointer_postprocessing_aa;
            break;
        case 12:    
            self->muladd_func_ptr = MatrixPointer_postprocessing_revaa;
            break;
        case 20:        
            self->muladd_func_ptr = MatrixPointer_postprocessing_ireva;
            break;
        case 21:    
            self->muladd_func_ptr = MatrixPointer_postprocessing_areva;
            break;
        case 22:    
            self->muladd_func_ptr = MatrixPointer_postprocessing_revareva;
            break;
    } 
}

static void
MatrixPointer_compute_next_data_frame(MatrixPointer *self)
{
    (*self->proc_func_ptr)(self); 
    (*self->muladd_func_ptr)(self);
    Stream_setData(self->stream, self->data);
}

static int
MatrixPointer_traverse(MatrixPointer *self, visitproc visit, void *arg)
{
    pyo_VISIT
    Py_VISIT(self->matrix);
    Py_VISIT(self->indexrow);    
    Py_VISIT(self->indexrow_stream);    
    Py_VISIT(self->indexcol);    
    Py_VISIT(self->indexcol_stream);    
    return 0;
}

static int 
MatrixPointer_clear(MatrixPointer *self)
{
    pyo_CLEAR
    Py_CLEAR(self->matrix);
    Py_CLEAR(self->indexrow);    
    Py_CLEAR(self->indexrow_stream);    
    Py_CLEAR(self->indexcol);    
    Py_CLEAR(self->indexcol_stream);    
    return 0;
}

static void
MatrixPointer_dealloc(MatrixPointer* self)
{
    free(self->data);
    MatrixPointer_clear(self);
    self->ob_type->tp_free((PyObject*)self);
}

static PyObject * MatrixPointer_deleteStream(MatrixPointer *self) { DELETE_STREAM };

static PyObject *
MatrixPointer_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    MatrixPointer *self;
    self = (MatrixPointer *)type->tp_alloc(type, 0);
    
	self->modebuffer[0] = 0;
	self->modebuffer[1] = 0;
    
    INIT_OBJECT_COMMON
    Stream_setFunctionPtr(self->stream, MatrixPointer_compute_next_data_frame);
    self->mode_func_ptr = MatrixPointer_setProcMode;
    
    return (PyObject *)self;
}

static int
MatrixPointer_init(MatrixPointer *self, PyObject *args, PyObject *kwds)
{
    PyObject *matrixtmp, *indexrowtmp, *indexcoltmp, *multmp=NULL, *addtmp=NULL;
    
    static char *kwlist[] = {"matrix", "indexrow", "indexcol", "mul", "add", NULL};
    
    if (! PyArg_ParseTupleAndKeywords(args, kwds, "OOO|OO", kwlist, &matrixtmp, &indexrowtmp, &indexcoltmp, &multmp, &addtmp))
        return -1; 
    
    Py_XDECREF(self->matrix);
    self->matrix = PyObject_CallMethod((PyObject *)matrixtmp, "getMatrixStream", "");
    
    if (indexrowtmp) {
        PyObject_CallMethod((PyObject *)self, "setIndexRow", "O", indexrowtmp);
    }

    if (indexcoltmp) {
        PyObject_CallMethod((PyObject *)self, "setIndexCol", "O", indexrowtmp);
    }
    
    PyObject_CallMethod((PyObject *)self, "setMul", "O", multmp);
    
    if (addtmp) {
        PyObject_CallMethod((PyObject *)self, "setAdd", "O", addtmp);
    }
    
    Py_INCREF(self->stream);
    PyObject_CallMethod(self->server, "addStream", "O", self->stream);
    
    (*self->mode_func_ptr)(self);
    
    MatrixPointer_compute_next_data_frame((MatrixPointer *)self);
    
    Py_INCREF(self);
    return 0;
}

static PyObject * MatrixPointer_getServer(MatrixPointer* self) { GET_SERVER };
static PyObject * MatrixPointer_getStream(MatrixPointer* self) { GET_STREAM };
static PyObject * MatrixPointer_setMul(MatrixPointer *self, PyObject *arg) { SET_MUL };	
static PyObject * MatrixPointer_setAdd(MatrixPointer *self, PyObject *arg) { SET_ADD };	
static PyObject * MatrixPointer_setSub(MatrixPointer *self, PyObject *arg) { SET_SUB };	
static PyObject * MatrixPointer_setDiv(MatrixPointer *self, PyObject *arg) { SET_DIV };	

static PyObject * MatrixPointer_play(MatrixPointer *self) { PLAY };
static PyObject * MatrixPointer_out(MatrixPointer *self, PyObject *args, PyObject *kwds) { OUT };
static PyObject * MatrixPointer_stop(MatrixPointer *self) { STOP };

static PyObject * MatrixPointer_multiply(MatrixPointer *self, PyObject *arg) { MULTIPLY };
static PyObject * MatrixPointer_inplace_multiply(MatrixPointer *self, PyObject *arg) { INPLACE_MULTIPLY };
static PyObject * MatrixPointer_add(MatrixPointer *self, PyObject *arg) { ADD };
static PyObject * MatrixPointer_inplace_add(MatrixPointer *self, PyObject *arg) { INPLACE_ADD };
static PyObject * MatrixPointer_sub(MatrixPointer *self, PyObject *arg) { SUB };
static PyObject * MatrixPointer_inplace_sub(MatrixPointer *self, PyObject *arg) { INPLACE_SUB };
static PyObject * MatrixPointer_div(MatrixPointer *self, PyObject *arg) { DIV };
static PyObject * MatrixPointer_inplace_div(MatrixPointer *self, PyObject *arg) { INPLACE_DIV };

static PyObject *
MatrixPointer_getMatrix(MatrixPointer* self)
{
    Py_INCREF(self->matrix);
    return self->matrix;
};

static PyObject *
MatrixPointer_setMatrix(MatrixPointer *self, PyObject *arg)
{
	PyObject *tmp;
	
	if (arg == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
    
	tmp = arg;
	Py_DECREF(self->matrix);
    self->matrix = PyObject_CallMethod((PyObject *)tmp, "getMatrixStream", "");
    
	Py_INCREF(Py_None);
	return Py_None;
}	

static PyObject *
MatrixPointer_setIndexRow(MatrixPointer *self, PyObject *arg)
{
	PyObject *tmp, *streamtmp;
	
	if (arg == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
    
	int isNumber = PyNumber_Check(arg);
	if (isNumber == 1) {
		printf("MatrixPointer indexrow attributes must be a PyoObject.\n");
        Py_INCREF(Py_None);
        return Py_None;
	}
	
	tmp = arg;
	Py_INCREF(tmp);
	Py_XDECREF(self->indexrow);

    self->indexrow = tmp;
    streamtmp = PyObject_CallMethod((PyObject *)self->indexrow, "_getStream", NULL);
    Py_INCREF(streamtmp);
    Py_XDECREF(self->indexrow_stream);
    self->indexrow_stream = (Stream *)streamtmp;
    
	Py_INCREF(Py_None);
	return Py_None;
}	

static PyObject *
MatrixPointer_setIndexCol(MatrixPointer *self, PyObject *arg)
{
	PyObject *tmp, *streamtmp;
	
	if (arg == NULL) {
		Py_INCREF(Py_None);
		return Py_None;
	}
    
	int isNumber = PyNumber_Check(arg);
	if (isNumber == 1) {
		printf("MatrixPointer indexcol attributes must be a PyoObject.\n");
        Py_INCREF(Py_None);
        return Py_None;
	}
	
	tmp = arg;
	Py_INCREF(tmp);
	Py_XDECREF(self->indexcol);
    
    self->indexcol = tmp;
    streamtmp = PyObject_CallMethod((PyObject *)self->indexcol, "_getStream", NULL);
    Py_INCREF(streamtmp);
    Py_XDECREF(self->indexcol_stream);
    self->indexcol_stream = (Stream *)streamtmp;
    
	Py_INCREF(Py_None);
	return Py_None;
}	

static PyMemberDef MatrixPointer_members[] = {
{"server", T_OBJECT_EX, offsetof(MatrixPointer, server), 0, "Pyo server."},
{"stream", T_OBJECT_EX, offsetof(MatrixPointer, stream), 0, "Stream object."},
{"matrix", T_OBJECT_EX, offsetof(MatrixPointer, matrix), 0, "Waveform matrix."},
{"indexrow", T_OBJECT_EX, offsetof(MatrixPointer, indexrow), 0, "Reader indexrow."},
{"indexcol", T_OBJECT_EX, offsetof(MatrixPointer, indexcol), 0, "Reader indexcol."},
{"mul", T_OBJECT_EX, offsetof(MatrixPointer, mul), 0, "Mul factor."},
{"add", T_OBJECT_EX, offsetof(MatrixPointer, add), 0, "Add factor."},
{NULL}  /* Sentinel */
};

static PyMethodDef MatrixPointer_methods[] = {
{"getMatrix", (PyCFunction)MatrixPointer_getMatrix, METH_NOARGS, "Returns waveform matrix object."},
{"getServer", (PyCFunction)MatrixPointer_getServer, METH_NOARGS, "Returns server object."},
{"_getStream", (PyCFunction)MatrixPointer_getStream, METH_NOARGS, "Returns stream object."},
{"deleteStream", (PyCFunction)MatrixPointer_deleteStream, METH_NOARGS, "Remove stream from server and delete the object."},
{"play", (PyCFunction)MatrixPointer_play, METH_NOARGS, "Starts computing without sending sound to soundcard."},
{"out", (PyCFunction)MatrixPointer_out, METH_VARARGS|METH_KEYWORDS, "Starts computing and sends sound to soundcard channel speficied by argument."},
{"stop", (PyCFunction)MatrixPointer_stop, METH_NOARGS, "Stops computing."},
{"setMatrix", (PyCFunction)MatrixPointer_setMatrix, METH_O, "Sets oscillator matrix."},
{"setIndexRow", (PyCFunction)MatrixPointer_setIndexRow, METH_O, "Sets reader indexrow."},
{"setIndexCol", (PyCFunction)MatrixPointer_setIndexCol, METH_O, "Sets reader indexcol."},
{"setMul", (PyCFunction)MatrixPointer_setMul, METH_O, "Sets oscillator mul factor."},
{"setAdd", (PyCFunction)MatrixPointer_setAdd, METH_O, "Sets oscillator add factor."},
{"setSub", (PyCFunction)MatrixPointer_setSub, METH_O, "Sets oscillator inverse add factor."},
{"setDiv", (PyCFunction)MatrixPointer_setDiv, METH_O, "Sets inverse mul factor."},
{NULL}  /* Sentinel */
};

static PyNumberMethods MatrixPointer_as_number = {
(binaryfunc)MatrixPointer_add,                      /*nb_add*/
(binaryfunc)MatrixPointer_sub,                 /*nb_subtract*/
(binaryfunc)MatrixPointer_multiply,                 /*nb_multiply*/
(binaryfunc)MatrixPointer_div,                   /*nb_divide*/
0,                /*nb_remainder*/
0,                   /*nb_divmod*/
0,                   /*nb_power*/
0,                  /*nb_neg*/
0,                /*nb_pos*/
0,                  /*(unaryfunc)array_abs,*/
0,                    /*nb_nonzero*/
0,                    /*nb_invert*/
0,               /*nb_lshift*/
0,              /*nb_rshift*/
0,              /*nb_and*/
0,              /*nb_xor*/
0,               /*nb_or*/
0,                                          /*nb_coerce*/
0,                       /*nb_int*/
0,                      /*nb_long*/
0,                     /*nb_float*/
0,                       /*nb_oct*/
0,                       /*nb_hex*/
(binaryfunc)MatrixPointer_inplace_add,              /*inplace_add*/
(binaryfunc)MatrixPointer_inplace_sub,         /*inplace_subtract*/
(binaryfunc)MatrixPointer_inplace_multiply,         /*inplace_multiply*/
(binaryfunc)MatrixPointer_inplace_div,           /*inplace_divide*/
0,        /*inplace_remainder*/
0,           /*inplace_power*/
0,       /*inplace_lshift*/
0,      /*inplace_rshift*/
0,      /*inplace_and*/
0,      /*inplace_xor*/
0,       /*inplace_or*/
0,             /*nb_floor_divide*/
0,              /*nb_true_divide*/
0,     /*nb_inplace_floor_divide*/
0,      /*nb_inplace_true_divide*/
0,                     /* nb_indexrow */
};

PyTypeObject MatrixPointerType = {
PyObject_HEAD_INIT(NULL)
0,                         /*ob_size*/
"_pyo.MatrixPointer_base",         /*tp_name*/
sizeof(MatrixPointer),         /*tp_basicsize*/
0,                         /*tp_itemsize*/
(destructor)MatrixPointer_dealloc, /*tp_dealloc*/
0,                         /*tp_print*/
0,                         /*tp_getattr*/
0,                         /*tp_setattr*/
0,                         /*tp_compare*/
0,                         /*tp_repr*/
&MatrixPointer_as_number,             /*tp_as_number*/
0,                         /*tp_as_sequence*/
0,                         /*tp_as_mapping*/
0,                         /*tp_hash */
0,                         /*tp_call*/
0,                         /*tp_str*/
0,                         /*tp_getattro*/
0,                         /*tp_setattro*/
0,                         /*tp_as_buffer*/
Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_CHECKTYPES, /*tp_flags*/
"MatrixPointer objects. Read a waveform matrix with a pointer indexrow.",           /* tp_doc */
(traverseproc)MatrixPointer_traverse,   /* tp_traverse */
(inquiry)MatrixPointer_clear,           /* tp_clear */
0,		               /* tp_richcompare */
0,		               /* tp_weaklistoffset */
0,		               /* tp_iter */
0,		               /* tp_iternext */
MatrixPointer_methods,             /* tp_methods */
MatrixPointer_members,             /* tp_members */
0,                      /* tp_getset */
0,                         /* tp_base */
0,                         /* tp_dict */
0,                         /* tp_descr_get */
0,                         /* tp_descr_set */
0,                         /* tp_dictoffset */
(initproc)MatrixPointer_init,      /* tp_init */
0,                         /* tp_alloc */
MatrixPointer_new,                 /* tp_new */
};
