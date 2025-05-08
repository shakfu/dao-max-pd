#include "m_pd.h"

/* The object structure
 * *******************************************************/
typedef struct _multy {
    t_object obj;
    t_float x_f;
} t_multy;

/* The arguments/inlets/outlets/vectors indexes
 * *******************************/
enum ARGUMENTS { NONE };
enum INLETS { I_INPUT1, I_INPUT2, NUM_INLETS };
enum OUTLETS { O_OUTPUT, NUM_OUTLETS };
enum DSP { PERFORM, OBJECT, INPUT1, INPUT2, OUTPUT, VECTOR_SIZE, NEXT };

/* The class pointer
 * **********************************************************/
static t_class* multy_class;

/* Function prototypes
 * ********************************************************/
void* multy_common_new(t_multy* x, short argc, t_atom* argv);
void multy_free(t_multy* x);
void multy_dsp(t_multy* x, t_signal** sp, short* count);
t_int* multy_perform(t_int* w);


/* Function prototypes
 * ********************************************************/
void* multy_new(t_symbol* s, short argc, t_atom* argv);

/* The 'initialization' routine
 * ***********************************************/
#ifdef WIN32
__declspec(dllexport) void multy_tilde_setup(void);
#endif
void multy_tilde_setup(void)
{
    /* Initialize the class */
    multy_class = class_new(gensym("multy~"), (t_newmethod)multy_new,
                            (t_method)multy_free, sizeof(t_multy), 0, A_GIMME,
                            0);

    /* Specify signal input, with automatic float to signal conversion */
    CLASS_MAINSIGNALIN(multy_class, t_multy, x_f);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(multy_class, (t_method)multy_dsp, gensym("dsp"), 0);

    /* Bind the object-specific methods */
    // nothing

    /* Print message to Max window */
    post("multy~ • External was loaded");
}

/* The 'new instance' routine
 * *************************************************/
void* multy_new(t_symbol* s, short argc, t_atom* argv)
{
    /* Instantiate a new object */
    t_multy* x = (t_multy*)pd_new(multy_class);

    return multy_common_new(x, argc, argv);
}

/* The common 'new instance' routine
 * ******************************************/
void* multy_common_new(t_multy* x, short argc, t_atom* argv)
{
    /* Create inlets */
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));

    /* Create signal outlets */
    outlet_new(&x->obj, gensym("signal"));

    /* Print message to Max window */
    post("multy~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine
 * ************************************************/
void multy_free(t_multy* x)
{
    /* Print message to Max window */
    post("multy~ • Memory was freed");
}

/* The 'DSP' method
 * ***********************************************************/
void multy_dsp(t_multy* x, t_signal** sp, short* count)
{
    /* Attach the object to the DSP chain */
    dsp_add(multy_perform, NEXT - 1, x, sp[0]->s_vec, sp[1]->s_vec,
            sp[2]->s_vec, sp[0]->s_n);

    /* Print message to Max window */
    post("multy~ • Executing 32-bit perform routine");
}

/* The 'perform' routine
 * ******************************************************/
t_int* multy_perform(t_int* w)
{
    /* Copy the object pointer */
    // t_multy *x = (t_multy *)w[OBJECT];

    /* Copy signal pointers */
    t_float* input1 = (t_float*)w[INPUT1];
    t_float* input2 = (t_float*)w[INPUT2];
    t_float* output = (t_float*)w[OUTPUT];

    /* Copy the signal vector size */
    t_int n = w[VECTOR_SIZE];

    /* Perform the DSP loop */
    while (n--) {
        *output++ = *input1++ * *input2++;
    }

    /* Return the next address in the DSP chain */
    return w + NEXT;
}
