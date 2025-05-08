#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

/* The global variables
 * *******************************************************/
#define MINIMUM_THRESHOLD 0.0
#define DEFAULT_THRESHOLD 0.5
#define MAXIMUM_THRESHOLD 1.0

#define MINIMUM_ATTENUATION 0.0
#define DEFAULT_ATTENUATION 0.1
#define MAXIMUM_ATTENUATION 1.0

/* The object structure
 * *******************************************************/
typedef struct _cleaner {
    t_pxobject obj;

    float fs;

    float threshold_value;
    float attenuation_value;

    short threshold_connected;
    short attenuation_connected;
} t_cleaner;

/* The arguments/inlets/outlets/vectors indexes
 * *******************************/
enum ARGUMENTS { A_THRESHOLD, A_ATTENUATION };
enum INLETS { I_INPUT, I_THRESHOLD, I_ATTENUATION, NUM_INLETS };
enum OUTLETS { O_OUTPUT, NUM_OUTLETS };
enum DSP {
    PERFORM,
    OBJECT,
    INPUT1,
    THRESHOLD,
    ATTENUATION,
    OUTPUT1,
    VECTOR_SIZE,
    NEXT
};

/* The class pointer
 * **********************************************************/
static t_class* cleaner_class;

/* Function prototypes
 * ********************************************************/
void* cleaner_common_new(t_cleaner* x, short argc, t_atom* argv);
void cleaner_free(t_cleaner* x);
void cleaner_dsp64(t_cleaner* x, t_object* dsp64, short* count,
                   double samplerate, long maxvectorsize, long flags);
void cleaner_perform64(t_cleaner* x, t_object* dsp64, double** ins,
                       long numins, double** outs, long numouts,
                       long sampleframes, long flags, void* userparam);

/* Function prototypes
 * ********************************************************/
void* cleaner_new(t_symbol* s, short argc, t_atom* argv);

void cleaner_float(t_cleaner* x, double farg);
void cleaner_assist(t_cleaner* x, void* b, long msg, long arg, char* dst);

/* The 'initialization' routine
 * ***********************************************/
int C74_EXPORT main()
{
    /* Initialize the class */
    cleaner_class = class_new("cleaner~", (method)cleaner_new,
                              (method)cleaner_free, sizeof(t_cleaner), 0,
                              A_GIMME, 0);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(cleaner_class, (method)cleaner_dsp64, "dsp64", A_CANT, 0);

    /* Bind the float method, which is called when floats are sent to inlets */
    class_addmethod(cleaner_class, (method)cleaner_float, "float", A_FLOAT, 0);

    /* Bind the assist method, which is called on mouse-overs to inlets and
     * outlets */
    class_addmethod(cleaner_class, (method)cleaner_assist, "assist", A_CANT,
                    0);

    /* Add standard Max methods to the class */
    class_dspinit(cleaner_class);

    /* Register the class with Max */
    class_register(CLASS_BOX, cleaner_class);

    /* Print message to Max window */
    object_post(NULL, "cleaner~ • External was loaded");

    /* Return with no error */
    return 0;
}

/* The 'new instance' routine
 * *************************************************/
void* cleaner_new(t_symbol* s, short argc, t_atom* argv)
{
    /* Instantiate a new object */
    t_cleaner* x = (t_cleaner*)object_alloc(cleaner_class);

    return cleaner_common_new(x, argc, argv);
}


/* The 'float' method
 * *********************************************************/
void cleaner_float(t_cleaner* x, double farg)
{
    long inlet = ((t_pxobject*)x)->z_in;

    switch (inlet) {
    case I_THRESHOLD:
        if (farg < MINIMUM_THRESHOLD) {
            farg = MINIMUM_THRESHOLD;
            object_warn((t_object*)x,
                        "cleaner~ • Invalid argument: Minimum threshold value "
                        "set to %.4f",
                        farg);
        } else if (farg > MAXIMUM_THRESHOLD) {
            farg = MAXIMUM_THRESHOLD;
            object_warn((t_object*)x,
                        "cleaner~ • Invalid argument: Maximum threshold value "
                        "set to %.4f",
                        farg);
        }
        x->threshold_value = farg;
        break;

    case I_ATTENUATION:
        if (farg < MINIMUM_ATTENUATION) {
            farg = MINIMUM_ATTENUATION;
            object_warn((t_object*)x,
                        "cleaner~ • Invalid argument: Minimum attenuation "
                        "value set to %.4f",
                        farg);
        } else if (farg > MAXIMUM_ATTENUATION) {
            farg = MAXIMUM_ATTENUATION;
            object_warn((t_object*)x,
                        "cleaner~ • Invalid argument: Maximum attenuation "
                        "value set to %.4f",
                        farg);
        }
        x->attenuation_value = farg;
        break;
    }
}

/* The 'assist' method
 * ********************************************************/
void cleaner_assist(t_cleaner* x, void* b, long msg, long arg, char* dst)
{
    /* Document inlet functions */
    if (msg == ASSIST_INLET) {
        switch (arg) {
        case I_INPUT:
            snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Input");
            break;
        case I_THRESHOLD:
            snprintf_zero(dst, ASSIST_MAX_STRING_LEN,
                          "(signal/float) Threshold");
            break;
        case I_ATTENUATION:
            snprintf_zero(dst, ASSIST_MAX_STRING_LEN,
                          "(signal/float) Attenuation");
            break;
        }
    }

    /* Document outlet functions */
    else if (msg == ASSIST_OUTLET) {
        switch (arg) {
        case O_OUTPUT:
            snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Output");
            break;
        }
    }
}


/* The common 'new instance' routine
 * ******************************************/
void* cleaner_common_new(t_cleaner* x, short argc, t_atom* argv)
{
    /* Create inlets */
    dsp_setup((t_pxobject*)x, NUM_INLETS);

    /* Create signal outlets */
    outlet_new((t_object*)x, "signal");

    /* Avoid sharing memory among audio vectors */
    x->obj.z_misc |= Z_NO_INPLACE;


    /* Initialize input arguments */
    float threshold_value = DEFAULT_THRESHOLD;
    float attenuation_value = DEFAULT_ATTENUATION;

    /* Parse passed arguments */
    if (argc > A_THRESHOLD) {
        threshold_value = atom_getfloatarg(A_THRESHOLD, argc, argv);
    }
    if (argc > A_ATTENUATION) {
        attenuation_value = atom_getfloatarg(A_ATTENUATION, argc, argv);
    }

    /* Check validity of passed arguments */
    if (threshold_value < MINIMUM_THRESHOLD) {
        threshold_value = MINIMUM_THRESHOLD;
        post(
            "cleaner~ • Invalid argument: Minimum threshold value set to %.4f",
            threshold_value);
    } else if (threshold_value > MAXIMUM_THRESHOLD) {
        threshold_value = MAXIMUM_THRESHOLD;
        post(
            "cleaner~ • Invalid argument: Maximum threshold value set to %.4f",
            threshold_value);
    }
    if (attenuation_value < MINIMUM_ATTENUATION) {
        attenuation_value = MINIMUM_ATTENUATION;
        post("cleaner~ • Invalid argument: Minimum attenuation value set to "
             "%.4f",
             attenuation_value);
    } else if (attenuation_value > MAXIMUM_ATTENUATION) {
        attenuation_value = MAXIMUM_ATTENUATION;
        post("cleaner~ • Invalid argument: Maximum attenuation value set to "
             "%.4f",
             attenuation_value);
    }

    /* Initialize some state variables */
    x->fs = sys_getsr();
    x->threshold_value = threshold_value;
    x->attenuation_value = attenuation_value;

    /* Print message to Max window */
    post("cleaner~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine
 * ************************************************/
void cleaner_free(t_cleaner* x)
{
    /* Remove the object from the DSP chain */
    dsp_free((t_pxobject*)x);

    /* Print message to Max window */
    post("cleaner~ • Memory was freed");
}

/******************************************************************************/

void cleaner_dsp64(t_cleaner* x, t_object* dsp64, short* count,
                   double samplerate, long maxvectorsize, long flags)
{
    /* Store signal connection states of inlets */
    x->threshold_connected = count[I_THRESHOLD];
    x->attenuation_connected = count[I_ATTENUATION];

    object_method(dsp64, gensym("dsp_add64"), x, cleaner_perform64, 0, NULL);
}

void cleaner_perform64(t_cleaner* x, t_object* dsp64, double** ins,
                       long numins, double** outs, long numouts,
                       long sampleframes, long flags, void* userparam)
{
    t_double* input = ins[0];
    t_double* threshold = ins[1];
    t_double* attenuation = ins[2];

    t_double* output = outs[0];
    int n = sampleframes;

    /* Perform the DSP loop */
    t_double maxamp = 0.0;
    t_double threshold_value;
    t_double attenuation_value;

    for (int ii = 0; ii < n; ii++) {
        if (maxamp < input[ii]) {
            maxamp = input[ii];
        }
    }

    if (x->threshold_connected) {
        threshold_value = (*threshold) * maxamp;
    } else {
        threshold_value = x->threshold_value * maxamp;
    }

    if (x->attenuation_connected) {
        attenuation_value = (*attenuation);
    } else {
        attenuation_value = x->attenuation_value;
    }

    for (int ii = 0; ii < n; ii++) {
        if (input[ii] < threshold_value) {
            input[ii] *= attenuation_value;
        }
        output[ii] = input[ii];
    }
}
