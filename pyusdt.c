#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include "usdt.h"

/* Global reference to sys.monitoring.MISSING */
static PyObject *MISSING = NULL;

/* Global state for dynamic monitoring */
static PyObject *g_module = NULL;
static PyObject *g_monitoring = NULL;
static int g_tool_id = -1;
static int g_monitoring_enabled = 0;
static pthread_t g_poll_thread;
static volatile int g_poll_thread_running = 0;

/* Helper to check if object is sys.monitoring.MISSING */
static int is_missing(PyObject *obj)
{
	return obj == MISSING;
}

/* Helper to extract code object info */
static int get_code_info(PyObject *code_obj, const char **func_name, const char **filename, int *lineno)
{
	/* Check if code_obj is MISSING */
	if (is_missing(code_obj)) {
		*func_name = "<missing>";
		*filename = "<missing>";
		*lineno = 0;
		return 0;
	}

	PyObject *name = PyObject_GetAttrString(code_obj, "co_name");
	PyObject *file = PyObject_GetAttrString(code_obj, "co_filename");
	PyObject *line = PyObject_GetAttrString(code_obj, "co_firstlineno");

	if (!name || !file || !line) {
		Py_XDECREF(name);
		Py_XDECREF(file);
		Py_XDECREF(line);
		return -1;
	}

	*func_name = PyUnicode_AsUTF8(name);
	*filename = PyUnicode_AsUTF8(file);
	*lineno = PyLong_AsLong(line);

	Py_DECREF(name);
	Py_DECREF(file);
	Py_DECREF(line);

	if (!*func_name || !*filename || *lineno == -1) {
		if (PyErr_Occurred())
			return -1;
	}

	return 0;
}

/* PY_START and PY_RESUME: callback(code, instruction_offset) */
static PyObject *py_start_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	long offset;
	const char *function_name;
	const char *filename;
	int line_number;

	/* Early return if not being traced */
	if (!USDT_IS_ACTIVE(pyusdt, PY_START))
		Py_RETURN_NONE;

	if (!PyArg_ParseTuple(args, "Ol", &code_obj, &offset))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &line_number) < 0)
		return NULL;

	USDT_WITH_SEMA(pyusdt, PY_START, function_name, filename, line_number, offset);
	Py_RETURN_NONE;
}

static PyObject *py_resume_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	long offset;
	const char *function_name;
	const char *filename;
	int line_number;

	/* Early return if not being traced */
	if (!USDT_IS_ACTIVE(pyusdt, PY_RESUME))
		Py_RETURN_NONE;

	if (!PyArg_ParseTuple(args, "Ol", &code_obj, &offset))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &line_number) < 0)
		return NULL;

	USDT_WITH_SEMA(pyusdt, PY_RESUME, function_name, filename, line_number, offset);
	Py_RETURN_NONE;
}

/* PY_RETURN and PY_YIELD: callback(code, instruction_offset, retval) */
static PyObject *py_return_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	PyObject *retval;
	long offset;
	const char *function_name;
	const char *filename;
	int line_number;
	const char *retval_repr;

	/* Early return if not being traced - avoid expensive PyObject_Repr() */
	if (!USDT_IS_ACTIVE(pyusdt, PY_RETURN))
		Py_RETURN_NONE;

	if (!PyArg_ParseTuple(args, "OlO", &code_obj, &offset, &retval))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &line_number) < 0)
		return NULL;

	/* Get string representation of return value */
	PyObject *repr = PyObject_Repr(retval);
	if (repr) {
		retval_repr = PyUnicode_AsUTF8(repr);
		if (retval_repr) {
			USDT_WITH_SEMA(pyusdt, PY_RETURN, function_name, filename, line_number, offset, retval_repr);
		}
		Py_DECREF(repr);
	} else {
		/* Clear the error and continue */
		PyErr_Clear();
	}

	Py_RETURN_NONE;
}

static PyObject *py_yield_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	PyObject *retval;
	long offset;
	const char *function_name;
	const char *filename;
	int line_number;
	const char *retval_repr;

	/* Early return if not being traced - avoid expensive PyObject_Repr() */
	if (!USDT_IS_ACTIVE(pyusdt, PY_YIELD))
		Py_RETURN_NONE;

	if (!PyArg_ParseTuple(args, "OlO", &code_obj, &offset, &retval))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &line_number) < 0)
		return NULL;

	/* Get string representation of yielded value */
	PyObject *repr = PyObject_Repr(retval);
	if (repr) {
		retval_repr = PyUnicode_AsUTF8(repr);
		if (retval_repr) {
			USDT_WITH_SEMA(pyusdt, PY_YIELD, function_name, filename, line_number, offset, retval_repr);
		}
		Py_DECREF(repr);
	} else {
		/* Clear the error and continue */
		PyErr_Clear();
	}

	Py_RETURN_NONE;
}

/* CALL: callback(code, instruction_offset, callable, arg0) */
static PyObject *call_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	PyObject *callable;
	PyObject *arg0;
	long offset;
	const char *function_name;
	const char *filename;
	int line_number;
	const char *callable_repr;

	/* Early return if not being traced - avoid expensive PyObject_Repr() */
	if (!USDT_IS_ACTIVE(pyusdt, CALL))
		Py_RETURN_NONE;

	if (!PyArg_ParseTuple(args, "OlOO", &code_obj, &offset, &callable, &arg0))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &line_number) < 0)
		return NULL;

	/* Get string representation of callable */
	PyObject *repr = PyObject_Repr(callable);
	if (repr) {
		callable_repr = PyUnicode_AsUTF8(repr);
		if (callable_repr) {
			USDT_WITH_SEMA(pyusdt, CALL, function_name, filename, line_number, offset, callable_repr);
		}
		Py_DECREF(repr);
	} else {
		/* Clear the error and continue */
		PyErr_Clear();
	}

	Py_RETURN_NONE;
}

/* LINE: callback(code, line_number) */
static PyObject *line_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	int line_number;
	const char *function_name;
	const char *filename;
	int first_line;

	/* Early return if not being traced */
	if (!USDT_IS_ACTIVE(pyusdt, LINE))
		Py_RETURN_NONE;

	if (!PyArg_ParseTuple(args, "Oi", &code_obj, &line_number))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &first_line) < 0)
		return NULL;

	USDT_WITH_SEMA(pyusdt, LINE, function_name, filename, line_number);
	Py_RETURN_NONE;
}

static PyMethodDef PyUSDTMethods[] = {
	{"_py_start_callback", py_start_callback, METH_VARARGS, "PY_START callback"},
	{"_py_resume_callback", py_resume_callback, METH_VARARGS, "PY_RESUME callback"},
	{"_py_return_callback", py_return_callback, METH_VARARGS, "PY_RETURN callback"},
	{"_py_yield_callback", py_yield_callback, METH_VARARGS, "PY_YIELD callback"},
	{"_call_callback", call_callback, METH_VARARGS, "CALL callback"},
	{"_line_callback", line_callback, METH_VARARGS, "LINE callback"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef pyusdtmodule = {
	PyModuleDef_HEAD_INIT,
	"libpyusdt",
	"USDT probe support for Python profiling",
	-1,
	PyUSDTMethods
};

/* Helper function to register a single event callback */
static int register_event_callback(PyObject *module, PyObject *monitoring, int tool_id,
                                     const char *event_name, const char *callback_name)
{
	PyObject *events;
	PyObject *event;
	PyObject *callback;
	PyObject *result;

	/* Get events object */
	events = PyObject_GetAttrString(monitoring, "events");
	if (!events)
		return -1;

	/* Get specific event */
	event = PyObject_GetAttrString(events, event_name);
	Py_DECREF(events);
	if (!event)
		return -1;

	/* Get callback function from module */
	callback = PyObject_GetAttrString(module, callback_name);
	if (!callback) {
		Py_DECREF(event);
		return -1;
	}

	/* Register callback */
	result = PyObject_CallMethod(monitoring, "register_callback", "iOO", tool_id, event, callback);
	Py_DECREF(callback);
	Py_DECREF(event);

	if (!result)
		return -1;

	Py_DECREF(result);
	return 0;
}

/* Check if any USDT semaphore is active */
static int any_semaphore_active(void)
{
	return USDT_IS_ACTIVE(pyusdt, PY_START) ||
	       USDT_IS_ACTIVE(pyusdt, PY_RESUME) ||
	       USDT_IS_ACTIVE(pyusdt, PY_RETURN) ||
	       USDT_IS_ACTIVE(pyusdt, PY_YIELD) ||
	       USDT_IS_ACTIVE(pyusdt, CALL) ||
	       USDT_IS_ACTIVE(pyusdt, LINE);
}

/* Enable monitoring by registering callbacks */
static int enable_monitoring(void)
{
	PyObject *events;
	PyObject *result;
	int event_mask;

	if (g_monitoring_enabled)
		return 0;  /* Already enabled */

	/* Build event mask for all 6 events */
	events = PyObject_GetAttrString(g_monitoring, "events");
	if (!events)
		return -1;

	event_mask = 0;
	PyObject *py_start = PyObject_GetAttrString(events, "PY_START");
	PyObject *py_resume = PyObject_GetAttrString(events, "PY_RESUME");
	PyObject *py_return = PyObject_GetAttrString(events, "PY_RETURN");
	PyObject *py_yield = PyObject_GetAttrString(events, "PY_YIELD");
	PyObject *call = PyObject_GetAttrString(events, "CALL");
	PyObject *line = PyObject_GetAttrString(events, "LINE");

	if (py_start) event_mask |= PyLong_AsLong(py_start);
	if (py_resume) event_mask |= PyLong_AsLong(py_resume);
	if (py_return) event_mask |= PyLong_AsLong(py_return);
	if (py_yield) event_mask |= PyLong_AsLong(py_yield);
	if (call) event_mask |= PyLong_AsLong(call);
	if (line) event_mask |= PyLong_AsLong(line);

	Py_XDECREF(py_start);
	Py_XDECREF(py_resume);
	Py_XDECREF(py_return);
	Py_XDECREF(py_yield);
	Py_XDECREF(call);
	Py_XDECREF(line);
	Py_DECREF(events);

	/* Set all events */
	result = PyObject_CallMethod(g_monitoring, "set_events", "ii", g_tool_id, event_mask);
	if (!result)
		return -1;
	Py_DECREF(result);

	/* Register callbacks for each event */
	if (register_event_callback(g_module, g_monitoring, g_tool_id, "PY_START", "_py_start_callback") < 0 ||
	    register_event_callback(g_module, g_monitoring, g_tool_id, "PY_RESUME", "_py_resume_callback") < 0 ||
	    register_event_callback(g_module, g_monitoring, g_tool_id, "PY_RETURN", "_py_return_callback") < 0 ||
	    register_event_callback(g_module, g_monitoring, g_tool_id, "PY_YIELD", "_py_yield_callback") < 0 ||
	    register_event_callback(g_module, g_monitoring, g_tool_id, "CALL", "_call_callback") < 0 ||
	    register_event_callback(g_module, g_monitoring, g_tool_id, "LINE", "_line_callback") < 0) {
		return -1;
	}

	g_monitoring_enabled = 1;
	PySys_WriteStderr("pyusdt: monitoring enabled\n");
	return 0;
}

/* Disable monitoring by deregistering callbacks */
static int disable_monitoring(void)
{
	PyObject *result;

	if (!g_monitoring_enabled)
		return 0;  /* Already disabled */

	/* Set events to 0 (disable all) */
	result = PyObject_CallMethod(g_monitoring, "set_events", "ii", g_tool_id, 0);
	if (!result)
		return -1;
	Py_DECREF(result);

	g_monitoring_enabled = 0;
	PySys_WriteStderr("pyusdt: monitoring disabled\n");
	return 0;
}

/* Get polling interval from environment variable, default 100ms */
static int get_poll_interval_usec(void)
{
	const char *env = getenv("PYUSDT_CHECK_MSEC");
	if (env) {
		int msec = atoi(env);
		if (msec > 0 && msec <= 10000) {  /* Cap at 10 seconds */
			return msec * 1000;  /* Convert to microseconds */
		}
	}
	return 100000;  /* Default: 100ms */
}

/* Background thread that polls semaphores and enables/disables monitoring */
static void *poll_semaphores_thread(void *arg)
{
	(void)arg;
	int poll_interval = get_poll_interval_usec();

	while (g_poll_thread_running) {
		/* Check if any semaphore is active */
		int active = any_semaphore_active();

		/* Acquire GIL for Python API calls */
		PyGILState_STATE gstate = PyGILState_Ensure();

		if (active && !g_monitoring_enabled) {
			enable_monitoring();
		} else if (!active && g_monitoring_enabled) {
			disable_monitoring();
		}

		PyGILState_Release(gstate);

		/* Sleep before checking again */
		usleep(poll_interval);
	}

	return NULL;
}

PyMODINIT_FUNC PyInit_libpyusdt(void)
{
	PyObject *module;
	PyObject *sys_module;
	PyObject *monitoring;
	PyObject *profiler_id;
	PyObject *result;
	int tool_id;

	module = PyModule_Create(&pyusdtmodule);
	if (module == NULL)
		return NULL;

	/* Import sys.monitoring */
	sys_module = PyImport_ImportModule("sys");
	if (!sys_module) {
		Py_DECREF(module);
		return NULL;
	}

	monitoring = PyObject_GetAttrString(sys_module, "monitoring");
	Py_DECREF(sys_module);
	if (!monitoring) {
		Py_DECREF(module);
		return NULL;
	}

	/* Get reference to sys.monitoring.MISSING */
	MISSING = PyObject_GetAttrString(monitoring, "MISSING");
	if (!MISSING) {
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}
	/* Keep MISSING as a borrowed reference - don't DECREF */

	/* Get PROFILER_ID */
	profiler_id = PyObject_GetAttrString(monitoring, "PROFILER_ID");
	if (!profiler_id) {
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}
	tool_id = PyLong_AsLong(profiler_id);
	Py_DECREF(profiler_id);

	/* Register tool */
	result = PyObject_CallMethod(monitoring, "use_tool_id", "is", tool_id, "pyusdt-profiling");
	if (!result) {
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}
	Py_DECREF(result);

	/* Store global state for dynamic enable/disable */
	g_module = module;
	Py_INCREF(g_module);
	g_monitoring = monitoring;
	Py_INCREF(g_monitoring);
	g_tool_id = tool_id;

	/* Start background thread to poll semaphores */
	g_poll_thread_running = 1;
	if (pthread_create(&g_poll_thread, NULL, poll_semaphores_thread, NULL) != 0) {
		PySys_WriteStderr("pyusdt: failed to create poll thread\n");
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}

	PySys_WriteStderr("pyusdt: ready (monitoring will enable when tracer attaches)\n");

	return module;
}
