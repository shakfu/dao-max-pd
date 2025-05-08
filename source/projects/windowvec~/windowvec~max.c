#include "ext.h"
#include "z_dsp.h"
#include "ext_obex.h"

#include <math.h>
#include <stdlib.h>

/* The global variables *******************************************************/


/* The object structure *******************************************************/
typedef struct _windowvec {
    t_pxobject obj;
    float fs;

    float* window;
    int vecsize;
} t_windowvec;

/* The arguments/inlets/outlets/vectors indexes *******************************/
enum ARGUMENTS { NONE };
enum INLETS { I_INPUT, NUM_INLETS };
enum OUTLETS { O_OUTPUT, NUM_OUTLETS };
enum DSP { PERFORM, OBJECT,
           INPUT1, OUTPUT1,
           VECTOR_SIZE, NEXT };

/* The class pointer **********************************************************/
static t_class *windowvec_class;

/* Function prototypes ********************************************************/
void *windowvec_common_new(t_windowvec *x, short argc, t_atom *argv);
void windowvec_free(t_windowvec *x);
void windowvec_dsp64(t_windowvec* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void windowvec_perform64(t_windowvec* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

/******************************************************************************/


/* Function prototypes ********************************************************/
void *windowvec_new(t_symbol *s, short argc, t_atom *argv);

void windowvec_float(t_windowvec *x, double farg);
void windowvec_assist(t_windowvec *x, void *b, long msg, long arg, char *dst);

/* The 'initialization' routine ***********************************************/
int C74_EXPORT main()
{
    /* Initialize the class */
    windowvec_class = class_new("windowvec~",
                                (method)windowvec_new,
                                (method)windowvec_free,
                                sizeof(t_windowvec), 0, A_GIMME, 0);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(windowvec_class, (method)windowvec_dsp64, "dsp64", A_CANT, 0);

    /* Bind the float method, which is called when floats are sent to inlets */
    class_addmethod(windowvec_class, (method)windowvec_float, "float", A_FLOAT, 0);

    /* Bind the assist method, which is called on mouse-overs to inlets and outlets */
    class_addmethod(windowvec_class, (method)windowvec_assist, "assist", A_CANT, 0);

    /* Add standard Max methods to the class */
    class_dspinit(windowvec_class);

    /* Register the class with Max */
    class_register(CLASS_BOX, windowvec_class);

    /* Print message to Max window */
    object_post(NULL, "windowvec~ • External was loaded");

    /* Return with no error */
    return 0;
}

/* The 'new instance' routine *************************************************/
void *windowvec_new(t_symbol *s, short argc, t_atom *argv)
{
    /* Instantiate a new object */
    t_windowvec *x = (t_windowvec *)object_alloc(windowvec_class);

    return windowvec_common_new(x, argc, argv);
}

/* The 'float' method *********************************************************/
void windowvec_float(t_windowvec *x, double farg)
{
    // nothing
}

/* The 'assist' method ********************************************************/
void windowvec_assist(t_windowvec *x, void *b, long msg, long arg, char *dst)
{
    /* Document inlet functions */
    if (msg == ASSIST_INLET) {
        switch (arg) {
            case I_INPUT: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Input"); break;
        }
    }

    /* Document outlet functions */
    else if (msg == ASSIST_OUTLET) {
        switch (arg) {
            case O_OUTPUT: snprintf_zero(dst, ASSIST_MAX_STRING_LEN, "(signal) Output"); break;
        }
    }
}

/******************************************************************************/

/* The common 'new instance' routine ******************************************/
void *windowvec_common_new(t_windowvec *x, short argc, t_atom *argv)
{

    /* Create inlets */
    dsp_setup((t_pxobject *)x, NUM_INLETS);

    /* Create signal outlets */
    outlet_new((t_object *)x, "signal");

    /* Avoid sharing memory among audio vectors */
    x->obj.z_misc |= Z_NO_INPLACE;

    /* Initialize some state variables */
    x->fs = sys_getsr();

    x->window = NULL;
    x->vecsize = 128;

    /* Print message to Max window */
    post("windowvec~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine ************************************************/
void windowvec_free(t_windowvec *x)
{
    /* Remove the object from the DSP chain */
    dsp_free((t_pxobject *)x);

    /* Free allocated dynamic memory */
    if (x->window != NULL) {
        free(x->window);
    }

    /* Print message to Max window */
    post("windowvec~ • Memory was freed");
}

/* The 'DSP' method ***********************************************************/

void windowvec_dsp64(t_windowvec* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    int bytesize = 0;

    if (x->vecsize != samplerate) {
        x->vecsize = samplerate;

        bytesize = x->vecsize * sizeof(float);
        if (x->window == NULL) {
            x->window = (float *)malloc(bytesize);
        } else {
            x->window = (float *)realloc(x->window, bytesize);
        }

        for (int ii = 0; ii < x->vecsize; ii++) {
            x->window[ii] = -0.5 * cos(TWOPI * ii / (float)x->vecsize) + 0.5;
        }
    }

    /* Attach the object to the DSP chain */
    object_method(dsp64, gensym("dsp_add64"), x, windowvec_perform64, 0, NULL);

    /* Print message to Max window */
    post("windowvec~ • Executing 64-bit perform routine");
}

void windowvec_perform64(t_windowvec* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    t_double* input = ins[0];
    t_double* output = outs[0];
    int n = sampleframes;

    /* Load state variables */
    float *window = x->window;
    // t_double vecsize = x->vecsize;

    /* Perform the DSP loop */
    for (int ii = 0; ii < n; ii++) {
        output[ii] = input[ii] * window[ii];
    }
}
