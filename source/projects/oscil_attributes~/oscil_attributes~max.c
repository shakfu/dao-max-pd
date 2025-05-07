#include "ext.h"
#include "z_dsp.h"
#include "ext_obex.h"

#include <math.h>

/* The global variables *******************************************************/
#define MINIMUM_FREQUENCY 31.0
#define DEFAULT_FREQUENCY 300.0
#define MAXIMUM_FREQUENCY 8000.0

#define MINIMUM_TABLE_SIZE 4
#define DEFAULT_TABLE_SIZE 8192
#define MAXIMUM_TABLE_SIZE 1048576

#define DEFAULT_WAVEFORM "sine"

#define MINIMUM_HARMONICS 2
#define DEFAULT_HARMONICS 10
#define MAXIMUM_HARMONICS 1024

#define MINIMUM_CROSSFADE 0.0
#define DEFAULT_CROSSFADE 200.0
#define MAXIMUM_CROSSFADE 1000.0

#define NO_CROSSFADE 0
#define LINEAR_CROSSFADE 1
#define POWER_CROSSFADE 2

/* The object structure *******************************************************/
typedef struct _oscil_attributes {
    t_pxobject obj;
    float a_frequency;
    long a_crossfade_type;
    float a_crossfade_time;
    t_symbol *a_waveform;
    long a_harmonics;
    t_float *a_amplitudes;
    short frequency_connected;
    
    float frequency;
    long table_size;
    t_symbol *waveform;
    long harmonics;
    
    long harmonics_bl;
    
    long wavetable_bytes;
    float *wavetable;
    float *wavetable_old;
    long amplitudes_bytes;
    float *amplitudes;
    
    float fs;
    
    float phase;
    float increment;
    short dirty;
    
    short crossfade_type;
    float crossfade_time;
    long crossfade_samples;
    long crossfade_countdown;
    short crossfade_in_progress;
    short just_turned_on;
    
    float twopi;
    float piOtwo;
} t_oscil_attributes;

/* The arguments/inlets/outlets/vectors indexes *******************************/
enum ARGUMENTS { A_FREQUENCY, A_TABLE_SIZE, A_WAVEFORM, A_HARMONICS };
enum INLETS { I_FREQUENCY, NUM_INLETS };
enum OUTLETS { O_OUTPUT, NUM_OUTLETS };
enum DSP { PERFORM, OBJECT, FREQUENCY, OUTPUT, VECTOR_SIZE, NEXT };

/* The class pointer **********************************************************/
static t_class *oscil_attributes_class;

/* Function prototypes ********************************************************/
void *oscil_attributes_common_new(t_oscil_attributes *x, short argc, t_atom *argv);
void oscil_attributes_free(t_oscil_attributes *x);
void oscil_attributes_dsp64(t_oscil_attributes* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void oscil_attributes_perform64(t_oscil_attributes* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

void oscil_attributes_build_wavetable(t_oscil_attributes *x);
void oscil_attributes_build_sine(t_oscil_attributes *x);
void oscil_attributes_build_sawtooth(t_oscil_attributes *x);
void oscil_attributes_build_triangle(t_oscil_attributes *x);
void oscil_attributes_build_square(t_oscil_attributes *x);
void oscil_attributes_build_pulse(t_oscil_attributes *x);
void oscil_attributes_build_additive(t_oscil_attributes *x);
void oscil_attributes_build_list(t_oscil_attributes *x, t_symbol *msg, short argc, t_atom *argv);
void oscil_attributes_build_waveform(t_oscil_attributes *x);
void oscil_attributes_fadetime(t_oscil_attributes *x, t_symbol *msg, short argc, t_atom *argv);
void oscil_attributes_fadetype(t_oscil_attributes *x, t_symbol *msg, short argc, t_atom *argv);

/* Function prototypes ********************************************************/
void *oscil_attributes_new(t_symbol *s, short argc, t_atom *argv);

void oscil_attributes_float(t_oscil_attributes *x, double farg);
t_max_err a_frequency_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av);
t_max_err a_crossfade_type_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av);
t_max_err a_crossfade_time_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av);
t_max_err a_waveform_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av);
t_max_err a_harmonics_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av);
t_max_err a_amplitudes_get(t_oscil_attributes *x, void *attr, long *ac, t_atom **av);
t_max_err a_amplitudes_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av);
void oscil_attributes_assist(t_oscil_attributes *x, void *b, long msg, long arg, char *dst);

/* The 'initialization' routine ***********************************************/
int C74_EXPORT main()
{
	/* Initialize the class */
	oscil_attributes_class = class_new("oscil_attributes~",
                                       (method)oscil_attributes_new,
                                       (method)oscil_attributes_free,
                                       sizeof(t_oscil_attributes), 0, A_GIMME, 0);
	
	/* Bind the DSP method, which is called when the DACs are turned on */
	class_addmethod(oscil_attributes_class, (method)oscil_attributes_dsp64, "dsp64", A_CANT, 0);
	
	/* Bind the float method, which is called when floats are sent to inlets */
	class_addmethod(oscil_attributes_class, (method)oscil_attributes_float, "float", A_FLOAT, 0);
	
	/* Bind the assist method, which is called on mouse-overs to inlets and outlets */
	class_addmethod(oscil_attributes_class, (method)oscil_attributes_assist, "assist", A_CANT, 0);
    
    /* Bind the object-specific methods */
    class_addmethod(oscil_attributes_class, (method)oscil_attributes_build_sine, "sine", 0);
    class_addmethod(oscil_attributes_class, (method)oscil_attributes_build_triangle, "triangle", 0);
    class_addmethod(oscil_attributes_class, (method)oscil_attributes_build_sawtooth, "sawtooth", 0);
    class_addmethod(oscil_attributes_class, (method)oscil_attributes_build_square, "square", 0);
    class_addmethod(oscil_attributes_class, (method)oscil_attributes_build_pulse, "pulse", 0);
    class_addmethod(oscil_attributes_class, (method)oscil_attributes_build_list, "list", A_GIMME, 0);
    class_addmethod(oscil_attributes_class, (method)oscil_attributes_fadetime, "fadetime", A_GIMME, 0);
    class_addmethod(oscil_attributes_class, (method)oscil_attributes_fadetype, "fadetype", A_GIMME, 0);
	
	/* Add standard Max methods to the class */
	class_dspinit(oscil_attributes_class);
	
    /* Bind the attributes */
    CLASS_ATTR_FLOAT(oscil_attributes_class, "frequency", 0, t_oscil_attributes, a_frequency);
    CLASS_ATTR_LABEL(oscil_attributes_class, "frequency", 0, "Frequency");
    CLASS_ATTR_ORDER(oscil_attributes_class, "frequency", 0, "1");
    CLASS_ATTR_ACCESSORS(oscil_attributes_class, "frequency", NULL, a_frequency_set);

    CLASS_ATTR_LONG(oscil_attributes_class, "fadetype", 0, t_oscil_attributes, a_crossfade_type);
    CLASS_ATTR_LABEL(oscil_attributes_class, "fadetype", 0, "Crossfade type");
    CLASS_ATTR_ORDER(oscil_attributes_class, "fadetype", 0, "2");
    CLASS_ATTR_ENUMINDEX(oscil_attributes_class, "fadetype", 0,
                         "\"No Fade\" \"Linear\" \"Equal power\"");
    CLASS_ATTR_ACCESSORS(oscil_attributes_class, "fadetype", NULL, a_crossfade_type_set);

    CLASS_ATTR_FLOAT(oscil_attributes_class, "fadetime", 0, t_oscil_attributes, a_crossfade_time);
    CLASS_ATTR_LABEL(oscil_attributes_class, "fadetime", 0, "Crossfade time");
    CLASS_ATTR_ORDER(oscil_attributes_class, "fadetime", 0, "3");
    CLASS_ATTR_ACCESSORS(oscil_attributes_class, "fadetime", NULL, a_crossfade_time_set);

    CLASS_ATTR_SYM(oscil_attributes_class, "waveform", 0, t_oscil_attributes, a_waveform);
    CLASS_ATTR_LABEL(oscil_attributes_class, "waveform", 0, "Waveform");
    CLASS_ATTR_ORDER(oscil_attributes_class, "waveform", 0, "4");
    CLASS_ATTR_ENUM(oscil_attributes_class, "waveform", 0, "sine triangle sawtooth square pulse additive");
    CLASS_ATTR_ACCESSORS(oscil_attributes_class, "waveform", NULL, a_waveform_set);

    CLASS_ATTR_LONG(oscil_attributes_class, "harmonics", 0, t_oscil_attributes, a_harmonics);
    CLASS_ATTR_LABEL(oscil_attributes_class, "harmonics", 0, "Harmonics");
    CLASS_ATTR_ORDER(oscil_attributes_class, "harmonics", 0, "5");
    CLASS_ATTR_ACCESSORS(oscil_attributes_class, "harmonics", NULL, a_harmonics_set);

    CLASS_ATTR_FLOAT_ARRAY(oscil_attributes_class, "amplitudes", 0, t_oscil_attributes, a_amplitudes, MAXIMUM_HARMONICS);
    CLASS_ATTR_LABEL(oscil_attributes_class, "amplitudes", 0, "Amplitudes");
    CLASS_ATTR_ORDER(oscil_attributes_class, "amplitudes", 0, "6");
    CLASS_ATTR_ACCESSORS(oscil_attributes_class, "amplitudes", a_amplitudes_get, a_amplitudes_set);

	/* Register the class with Max */
	class_register(CLASS_BOX, oscil_attributes_class);
	
	/* Print message to Max window */
	object_post(NULL, "oscil_attributes~ • External was loaded");
	
	/* Return with no error */
	return 0;
}

/* The 'new instance' routine *************************************************/
void *oscil_attributes_new(t_symbol *s, short argc, t_atom *argv)
{
	/* Instantiate a new object */
	t_oscil_attributes *x = (t_oscil_attributes *)object_alloc(oscil_attributes_class);
    
	return oscil_attributes_common_new(x, argc, argv);
}

/* The 'float' method *********************************************************/
void oscil_attributes_float(t_oscil_attributes *x, double farg)
{
    long inlet = ((t_pxobject *)x)->z_in;
    
    switch (inlet) {
        case 0:
            if (farg < MINIMUM_FREQUENCY) {
                farg = MINIMUM_FREQUENCY;
                object_warn((t_object *)x, "Invalid argument: Frequency set to %d[ms]", (int)farg);
            }
            else if (farg > MAXIMUM_FREQUENCY) {
                farg = MAXIMUM_FREQUENCY;
                object_warn((t_object *)x, "Invalid argument: Frequency set to %d[ms]", (int)farg);
            }
            x->frequency = farg;
            break;
    }
    
    /* Print message to Max window */
    object_post((t_object *)x, "Receiving floats");
}

/* The 'setter' and 'getter' methods for attributes ***************************/
t_max_err a_frequency_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        x->a_frequency = atom_getfloat(av);
        x->frequency = x->a_frequency;
    }

    return MAX_ERR_NONE;
}

t_max_err a_crossfade_type_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        x->a_crossfade_type = atom_getfloat(av);
        x->crossfade_type = x->a_crossfade_type;
    }

    return MAX_ERR_NONE;
}

t_max_err a_crossfade_time_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        x->a_crossfade_time = atom_getfloat(av);
        x->crossfade_time = x->a_crossfade_time;
    }

    oscil_attributes_fadetime(x, NULL, ac, av);

    return MAX_ERR_NONE;
}

t_max_err a_waveform_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        x->a_waveform = atom_getsym(av);
        x->waveform = x->a_waveform;

        oscil_attributes_build_wavetable(x);
    }

    return MAX_ERR_NONE;
}

t_max_err a_harmonics_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        x->a_harmonics = atom_getfloat(av);
        x->harmonics = x->a_harmonics;

        oscil_attributes_build_wavetable(x);
    }

    return MAX_ERR_NONE;
}

t_max_err a_amplitudes_get(t_oscil_attributes *x, void *attr, long *ac, t_atom **av)
{
    if (!((*ac) && (*av))) {
        *ac = MAXIMUM_HARMONICS;
        if (!(*av = (t_atom *)sysmem_newptr(sizeof(t_atom) * (*ac)))) {
            *ac = 0;
            return MAX_ERR_OUT_OF_MEM;
        }
    }

    for (int ii = 0; ii < MAXIMUM_HARMONICS; ii++) {
        atom_setfloat(*av + ii, x->a_amplitudes[ii]);
    }

    return MAX_ERR_NONE;
}

t_max_err a_amplitudes_set(t_oscil_attributes *x, void *attr, long ac, t_atom *av)
{
    if (ac && av) {
        for (int ii = 0; ii < MAXIMUM_HARMONICS; ii++) {
            x->a_amplitudes[ii] = atom_getfloatarg(ii, ac, av);
        }
    }

    t_atom *rv = NULL;
    object_method_sym((t_object *)x, gensym("waveform"), gensym("additive"), rv);

    return MAX_ERR_NONE;
}

/* The 'assist' method ********************************************************/
void oscil_attributes_assist(t_oscil_attributes *x, void *b, long msg, long arg, char *dst)
{
    /* Document inlet functions */
    if (msg == ASSIST_INLET) {
        switch (arg) {
            case I_FREQUENCY: sprintf(dst, "(signal/float) Frequency"); break;
        }
    }
    
    /* Document outlet functions */
    else if (msg == ASSIST_OUTLET) {
        switch (arg) {
            case O_OUTPUT: sprintf(dst, "(signal) Output"); break;
        }
    }
}

/* The argument parsing functions *********************************************/
void parse_float_arg(float *variable,
                     float minimum_value,
                     float default_value,
                     float maximum_value,
                     int arg_index,
                     short argc,
                     t_atom *argv)
{
    *variable = default_value;
    
    if (argc > arg_index) { *variable = atom_getfloatarg(arg_index, argc, argv); }
    
    if (*variable < minimum_value) {
        *variable = minimum_value;
    }
    else if (*variable > maximum_value) {
        *variable = maximum_value;
    }
}

void parse_int_arg(long *variable,
                   long minimum_value,
                   long default_value,
                   long maximum_value,
                   int arg_index,
                   short argc,
                   t_atom *argv)
{
    *variable = default_value;
    
    if (argc > arg_index) { *variable = atom_getintarg(arg_index, argc, argv); }
    
    if (*variable < minimum_value) {
        *variable = minimum_value;
    }
    else if (*variable > maximum_value) {
        *variable = maximum_value;
    }
}

void parse_symbol_arg(t_symbol **variable,
                      t_symbol *default_value,
                      int arg_index,
                      short argc,
                      t_atom *argv)
{
    *variable = default_value;

    if (argc > arg_index) { *variable = atom_getsymarg(arg_index, argc, argv); }
}

/* The 'new' and 'delete' pointers functions **********************************/

void *new_memory(long nbytes)
{
    t_ptr pointer = sysmem_newptr(nbytes);
    
    if (pointer == NULL) {
        error("oscil_attributes~ • Cannot allocate memory for this object");
        return NULL;
    }
    return pointer;
}

void free_memory(void *ptr, long nbytes)
{
    sysmem_freeptr(ptr);
}

/* The common 'new instance' routine ******************************************/
void *oscil_attributes_common_new(t_oscil_attributes *x, short argc, t_atom *argv)
{
    /* Create signal inlets */
    dsp_setup((t_pxobject *)x, NUM_INLETS);
    
    /* Create signal outlets */
    outlet_new((t_object *)x, "signal");
    
    /* Avoid sharing memory among audio vectors */
    x->obj.z_misc |= Z_NO_INPLACE;
    
    /* Parse passed arguments */
    parse_float_arg(&x->frequency, MINIMUM_FREQUENCY, DEFAULT_FREQUENCY, MAXIMUM_FREQUENCY, A_FREQUENCY, argc, argv);
    parse_int_arg(&x->table_size, MINIMUM_TABLE_SIZE, DEFAULT_TABLE_SIZE, MAXIMUM_TABLE_SIZE, A_TABLE_SIZE, argc, argv);
    parse_symbol_arg(&x->waveform, gensym(DEFAULT_WAVEFORM), A_WAVEFORM, argc, argv);
    parse_int_arg(&x->harmonics, MINIMUM_HARMONICS, DEFAULT_HARMONICS, MAXIMUM_HARMONICS, A_HARMONICS, argc, argv);
    
    /* Initialize state variables */
    x->wavetable_bytes = x->table_size * sizeof(float);
    x->wavetable = (float *)new_memory(x->wavetable_bytes);
    x->wavetable_old = (float *)new_memory(x->wavetable_bytes);
    
    x->amplitudes_bytes = MAXIMUM_HARMONICS * sizeof(float);
    x->amplitudes = (float *)new_memory(x->amplitudes_bytes);
    for (int ii = 0; ii < MAXIMUM_HARMONICS; ii++) {
        x->amplitudes[ii] = 0.0;
    }

    x->a_amplitudes = (float *)new_memory(x->amplitudes_bytes);
    for (int ii = 0; ii < MAXIMUM_HARMONICS; ii++) {
        x->a_amplitudes[ii] = 0.0;
    }
    
    x->fs = sys_getsr();
    
    x->phase = 0;
    x->increment = (float)x->table_size / x->fs;
    x->dirty = 0;
    
    x->crossfade_type = POWER_CROSSFADE;
    x->crossfade_time = DEFAULT_CROSSFADE;
    x->crossfade_samples = x->crossfade_time * x->fs / 1000.0;
    x->crossfade_countdown = 0;
    x->crossfade_in_progress = 0;
    x->just_turned_on = 1;
    
    x->twopi = 8.0 * atan(1.0);
    x->piOtwo = 2.0 * atan(1.0);

    /* Process the attributes */
    x->a_frequency = x->frequency;
    x->a_crossfade_type = x->crossfade_type;
    x->a_waveform = x->waveform;
    x->a_harmonics = x->harmonics;
    x->a_crossfade_time = x->crossfade_time;
    attr_args_process(x, argc, argv);

    /* Build wavetable */
    oscil_attributes_build_wavetable(x);

    /* Print message to Max window */
    post("oscil_attributes~ • Object was created");
    
    /* Return a pointer to the new object */
    return x;
}

/* The object-specific methods ************************************************/
void oscil_attributes_build_wavetable(t_oscil_attributes *x)
{
    if (x->waveform == gensym("sine")) {
        x->waveform = gensym("");
        oscil_attributes_build_sine(x);
    } else if (x->waveform == gensym("triangle")) {
        x->waveform = gensym("");
        oscil_attributes_build_triangle(x);
    } else if (x->waveform == gensym("sawtooth")) {
        x->waveform = gensym("");
        oscil_attributes_build_sawtooth(x);
    } else if (x->waveform == gensym("square")) {
        x->waveform = gensym("");
        oscil_attributes_build_square(x);
    } else if (x->waveform == gensym("pulse")) {
        x->waveform = gensym("");
        oscil_attributes_build_pulse(x);
    } else if (x->waveform == gensym("additive")) {
        x->waveform = gensym("");
        oscil_attributes_build_additive(x);
    } else {
        x->waveform = gensym("");
        oscil_attributes_build_sine(x);
        
        error("oscil_attributes~ • Invalid argument: Waveform set to %s", x->waveform->s_name);
    }
}

void oscil_attributes_build_sine(t_oscil_attributes *x)
{
    if (x->waveform == gensym("sine")) { return; }
    
    x->harmonics_bl = x->harmonics;
    
    for (int ii = 0; ii < x->harmonics_bl; ii++) {
        x->amplitudes[ii] = 0.0;
    }
    x->amplitudes[1] = 1.0;
    
    oscil_attributes_build_waveform(x);
    x->waveform = gensym("sine");
}

void oscil_attributes_build_triangle(t_oscil_attributes *x)
{
    if (x->waveform == gensym("triangle")) { return; }
    
    x->harmonics_bl = x->harmonics;
    
    float sign = 1.0;
    for (int ii = 1; ii < x->harmonics_bl; ii += 2) {
        x->amplitudes[ii+0] = sign / ( (float)ii * (float)ii );
        x->amplitudes[ii+1] = 0.0;
        sign *= -1.0;
    }
    
    oscil_attributes_build_waveform(x);
    x->waveform = gensym("triangle");
}

void oscil_attributes_build_sawtooth(t_oscil_attributes *x)
{
    if (x->waveform == gensym("sawtooth")) { return; }
    
    x->harmonics_bl = x->harmonics;
    
    float sign = 1.0;
    for (int ii = 1; ii < x->harmonics_bl; ii++) {
        x->amplitudes[ii] = sign / (float)ii;
        sign *= -1.0;
    }
    
    oscil_attributes_build_waveform(x);
    x->waveform = gensym("sawtooth");
}
void oscil_attributes_build_square(t_oscil_attributes *x)
{
    if (x->waveform == gensym("square")) { return; }
    
    x->harmonics_bl = x->harmonics;
    
    for (int ii = 1; ii < x->harmonics_bl; ii += 2) {
        x->amplitudes[ii+0] = 1.0 / (float)ii;
        x->amplitudes[ii+1] = 0.0;
    }
    
    oscil_attributes_build_waveform(x);
    x->waveform = gensym("square");
}
void oscil_attributes_build_pulse(t_oscil_attributes *x)
{
    if (x->waveform == gensym("pulse")) { return; }
    
    x->harmonics_bl = x->harmonics;
    
    for (int ii = 1; ii < x->harmonics_bl; ii++) {
        x->amplitudes[ii] = 1.0;
    }
    
    oscil_attributes_build_waveform(x);
    x->waveform = gensym("pulse");
}

void oscil_attributes_build_additive(t_oscil_attributes *x)
{
    if (x->waveform == gensym("additive")) { return; }

    x->harmonics_bl = 0;
    for (int ii = 0; ii < MAXIMUM_HARMONICS; ii++) {
        x->amplitudes[ii] = x->a_amplitudes[ii];
        if (x->a_amplitudes != 0) {
            x->harmonics_bl++;
        }
    }

    oscil_attributes_build_waveform(x);
    x->waveform = gensym("additive");
}

void oscil_attributes_build_list(t_oscil_attributes *x, t_symbol *msg, short argc, t_atom *argv)
{
    x->harmonics_bl = 0;
    
    if (argc > MAXIMUM_HARMONICS) {
        argc = MAXIMUM_HARMONICS;
    }
    
    for (int ii = 0; ii < argc; ii++) {
        x->amplitudes[ii] = atom_getfloat(argv + ii);
        x->harmonics_bl++;
    }
    
    oscil_attributes_build_waveform(x);
    x->waveform = gensym("list");
}

void oscil_attributes_build_waveform(t_oscil_attributes *x)
{
    /* Load state variables */
    long table_size = x->table_size;
    long harmonics_bl = x->harmonics_bl;
    
    float *wavetable = x->wavetable;
    float *wavetable_old = x->wavetable_old;
    float *amplitudes = x->amplitudes;
    
    float twopi = x->twopi;
    
    /* Check for state of crossfade */
    if (x->crossfade_in_progress) {
        return;
    }
    
    /* Save a backup of the current wavetable */
    for (int ii = 0; ii < table_size; ii++) {
        wavetable_old[ii] = wavetable[ii];
    }
    
    x->dirty = 1;
    
        /* Initialize crossfade and wavetable */
        if (x->crossfade_type != NO_CROSSFADE && !x->just_turned_on) {
            x->crossfade_countdown = x->crossfade_samples;
            x->crossfade_in_progress = 1;
        } else {
            x->crossfade_countdown = 0;
            
            for (int ii = 0; ii < x->table_size; ii++) {
                x->wavetable[ii] = 0.0;
            }
        }
    
        /* Initialize (clear) wavetable with DC component */
        for (int ii = 0; ii < table_size; ii++) {
            wavetable[ii] = amplitudes[0];
        }
        
        /* Build the wavetable using additive synthesis */
        for (int jj = 1; jj < harmonics_bl; jj++) {
            if (amplitudes[jj]) {
                for (int ii = 0; ii < table_size; ii++) {
                    wavetable[ii] += amplitudes[jj] * sin( twopi * (float)ii * (float)jj / (float)table_size );
                }
            }
        }
        
        /* Normalize wavetable to a peak value of 1.0 */
        float max = 0.0;
        for (int ii = 0; ii < table_size; ii++) {
            if (max < fabs(wavetable[ii])) {
                max = fabs(wavetable[ii]);
            }
        }
        if (max != 0.0) {
            float rescale = 1.0/max;
            for (int ii = 0; ii < table_size; ii++) {
                wavetable[ii] *= rescale;
            }
        }
    
    x->dirty = 0;
    x->just_turned_on = 0;
}

void oscil_attributes_fadetime(t_oscil_attributes *x, t_symbol *msg, short argc, t_atom *argv)
{
    float crossfade_ms = atom_getfloat(argv);
    
    if (crossfade_ms < MINIMUM_CROSSFADE) {
        crossfade_ms = MINIMUM_CROSSFADE;
    }
    else if (crossfade_ms > MAXIMUM_CROSSFADE) {
        crossfade_ms = MAXIMUM_CROSSFADE;
    }
    
    x->crossfade_time = crossfade_ms;
    x->crossfade_samples = x->crossfade_time * x->fs / 1000;
}

void oscil_attributes_fadetype(t_oscil_attributes *x, t_symbol *msg, short argc, t_atom *argv)
{
    float crossfade_type = (short)atom_getfloat(argv);
    
    if (crossfade_type < NO_CROSSFADE) {
        crossfade_type = NO_CROSSFADE;
    }
    else if (crossfade_type > POWER_CROSSFADE) {
        crossfade_type = POWER_CROSSFADE;
    }
    
    x->crossfade_type = (short)crossfade_type;

    /* Process the attributes */
    x->a_crossfade_type = x->crossfade_type;
    attr_args_process(x, argc, argv);
}

/* The 'free instance' routine ************************************************/
void oscil_attributes_free(t_oscil_attributes *x)
{
    /* Remove the object from the DSP chain */
    dsp_free((t_pxobject *)x);

    /* Free allocated dynamic memory */
    free_memory(x->wavetable, x->wavetable_bytes);
    free_memory(x->wavetable_old, x->wavetable_bytes);
    free_memory(x->amplitudes, x->amplitudes_bytes);
    free_memory(x->a_amplitudes, x->amplitudes_bytes);


    /* Print message to Max window */
    post("oscil_attributes~ • Memory was freed");
}

/* The 'DSP' method ***********************************************************/

void oscil_attributes_dsp64(t_oscil_attributes* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    /* Store signal connection states of inlets */
    x->frequency_connected = count[0];
    
    /* Adjust to changes in the sampling rate */
    if (x->fs != samplerate) {
        x->fs = samplerate;
        
        x->increment = (t_double)x->table_size / x->fs;
        x->crossfade_samples = x->crossfade_time * x->fs / 1000.0;
    }
    x->phase = 0;
    x->just_turned_on = 1;
    
    /* Attach the object to the DSP chain */
    object_method(dsp64, gensym("dsp_add64"), x, oscil_attributes_perform64, 0, NULL);
    
    /* Print message to Max window */
    post("oscil_attributes~ • Executing 64-bit perform routine");
}

void oscil_attributes_perform64(t_oscil_attributes* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    t_double* frequency_signal = ins[0];
    t_double* output = outs[0];
    int n = sampleframes;
    
    /* Load state variables */
    t_double frequency = x->frequency;
    long table_size = x->table_size;
    
    float *wavetable = x->wavetable;
    float *wavetable_old = x->wavetable_old;
    
    t_double phase = x->phase;
    t_double increment = x->increment;
    
    short crossfade_type = x->crossfade_type;
    long crossfade_samples = x->crossfade_samples;
    long crossfade_countdown = x->crossfade_countdown;
    
    t_double piOtwo = x->piOtwo;
    
    /* Perform the DSP loop */
    t_double sample_increment;
    long iphase;
    
    t_double interp;
    t_double samp1;
    t_double samp2;
    
    t_double old_sample;
    t_double new_sample;
    t_double out_sample;
    t_double fraction;
    
    while (n--)
    {
        if (x->frequency_connected) {
            sample_increment = increment * *frequency_signal++;
        } else {
            sample_increment = increment * frequency;
        }
        
        iphase = floor(phase);
        interp = phase - iphase;
        
        samp1 = wavetable_old[ (iphase+0) ];
        samp2 = wavetable_old[ (iphase+1)%table_size ];
        old_sample = samp1 + interp * (samp2 - samp1);
        
        samp1 = wavetable[ (iphase+0) ];
        samp2 = wavetable[ (iphase+1)%table_size ];
        new_sample = samp1 + interp * (samp2 - samp1);
        
        if (x->dirty) {
            out_sample = old_sample;
        } else if (crossfade_countdown > 0) {
            fraction = (t_double)crossfade_countdown / (t_double)crossfade_samples;
            
            if (crossfade_type == POWER_CROSSFADE) {
                fraction *= piOtwo;
                out_sample = sin(fraction) * old_sample + cos(fraction) * new_sample;
            } else if (crossfade_type == LINEAR_CROSSFADE) {
                out_sample = fraction * old_sample + (1-fraction) * new_sample;
            } else {
                out_sample = old_sample;
            }
            
            crossfade_countdown--;
            
        } else {
            out_sample = new_sample;
        }
        
        *output++ = out_sample;
        
        phase += sample_increment;
        while (phase >= table_size)
            phase -= table_size;
        while (phase < 0)
            phase += table_size;
    }
    
    if (crossfade_countdown == 0) {
        x->crossfade_in_progress = 0;
    }
    
    /* Update state variables */
    x->phase = phase;
    x->crossfade_countdown = crossfade_countdown;
}
