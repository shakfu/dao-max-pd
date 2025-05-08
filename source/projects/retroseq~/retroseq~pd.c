#include "m_pd.h"

#include <time.h>
#include <stdlib.h>

/* The global variables *******************************************************/
#define MAXIMUM_SEQUENCE_LENGTH 1024
#define DEFAULT_SEQUENCE_LENGTH 3

#define F0 440
#define F1 550
#define F2 660

#define D0 250
#define D1 125
#define D2 125

#define DEFAULT_TEMPO_BPM 15

#define DEFAULT_SUSTAIN_AMPLITUDE 0.7
#define DEFAULT_ATACK_DURATION 20
#define DEFAULT_DECAY_DURATION 50
#define DEFAULT_SUSTAIN_DURATION 100
#define DEFAULT_RELEASE_DURATION 50

/* The object structure *******************************************************/
typedef struct _retroseq {
    t_object obj;
    t_float x_f;

    float fs;

    int max_sequence_bytes;

    float *note_sequence;
    int note_sequence_length;
    float current_note_value;
    int note_counter;

    float *duration_sequence;
    int duration_sequence_length;
    float current_duration_value;
    int duration_counter;

    void *shuffle_freqs_outlet;
    void *shuffle_durs_outlet;
    t_atom *shuffle_list;

    float tempo_bpm;
    float duration_factor;
    int sample_counter;

    void *bang_outlet;
    void *bang_clock;

    void *adsr_outlet;
    void *adsr_clock;

    short elastic_sustain;
    float sustain_amplitude;

    int adsr_bytes;
    float *adsr;
    int adsr_out_bytes;
    float *adsr_out;
    int adsr_list_bytes;
    t_atom *adsr_list;

    short manual_override;
    short trigger_sent;
    short play_backwards;
} t_retroseq;

/* The arguments/inlets/outlets/vectors indexes *******************************/
enum ARGUMENTS { A_NONE };
enum INLETS { NUM_INLETS };
enum OUTLETS { O_OUTPUT, O_ADSR, O_BANG, O_SHUFFLE_F, O_SHUFFLE_D, NUM_OUTLETS };
enum DSP { PERFORM,
           OBJECT, OUTPUT, VECTOR_SIZE,
           NEXT };

/* The class pointer **********************************************************/
static t_class *retroseq_class;

/* Function prototypes ********************************************************/
void *retroseq_common_new(t_retroseq *x, short argc, t_atom *argv);
void retroseq_free(t_retroseq *x);
#ifdef TARGET_IS_MAX
void retroseq_dsp64(t_retroseq* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void retroseq_perform64(t_retroseq* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);
#else
void retroseq_dsp(t_retroseq *x, t_signal **sp, short *count);
t_int *retroseq_perform(t_int *w);
#endif
/* The object-specific prototypes *********************************************/
void retroseq_list(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv);
void retroseq_freqlist(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv);
void retroseq_durlist(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv);

void retroseq_shuffle_freqs(t_retroseq *x);
void retroseq_shuffle_durs(t_retroseq *x);
void retroseq_shuffle(t_retroseq *x);

void retroseq_set_tempo(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv);
void retroseq_set_elastic_sustain(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv);
void retroseq_set_sustain_amplitude(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv);
void retroseq_set_adsr(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv);

void retroseq_send_adsr(t_retroseq *x);
void retroseq_send_bang(t_retroseq *x);

void retroseq_manual_override(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv);
void retroseq_trigger_sent(t_retroseq *x);
void retroseq_play_backwards(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv);

/******************************************************************************/

/* Function prototypes ********************************************************/
void *retroseq_new(t_symbol *s, short argc, t_atom *argv);

/* The 'initialization' routine ***********************************************/
#ifdef WIN32
__declspec(dllexport) void retroseq_tilde_setup(void);
#endif
void retroseq_tilde_setup(void)
{
    /* Initialize the class */
    retroseq_class = class_new(gensym("retroseq~"),
                               (t_newmethod)retroseq_new,
                               (t_method)retroseq_free,
                               sizeof(t_retroseq), 0, A_GIMME, 0);

    /* Specify signal input, with automatic float to signal conversion */
    CLASS_MAINSIGNALIN(retroseq_class, t_retroseq, x_f);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(retroseq_class, (t_method)retroseq_dsp, gensym("dsp"), 0);

    /* Bind the object-specific methods */
    class_addmethod(retroseq_class, (t_method)retroseq_list, gensym("list"), A_GIMME, 0);
    class_addmethod(retroseq_class, (t_method)retroseq_freqlist, gensym("freqlist"), A_GIMME, 0);
    class_addmethod(retroseq_class, (t_method)retroseq_durlist, gensym("durlist"), A_GIMME, 0);

    class_addmethod(retroseq_class, (t_method)retroseq_shuffle_freqs, gensym("shuffle_freqs"), 0);
    class_addmethod(retroseq_class, (t_method)retroseq_shuffle_durs, gensym("shuffle_durs"), 0);
    class_addmethod(retroseq_class, (t_method)retroseq_shuffle, gensym("shuffle"), 0);

    class_addmethod(retroseq_class, (t_method)retroseq_set_tempo, gensym("tempo"), A_GIMME, 0);
    class_addmethod(retroseq_class, (t_method)retroseq_set_elastic_sustain, gensym("elastic_sustain"), A_GIMME, 0);
    class_addmethod(retroseq_class, (t_method)retroseq_set_sustain_amplitude, gensym("sustain_amplitude"), A_GIMME, 0);
    class_addmethod(retroseq_class, (t_method)retroseq_set_adsr, gensym("adsr"), A_GIMME, 0);

    class_addmethod(retroseq_class, (t_method)retroseq_manual_override, gensym("manual_override"), A_GIMME, 0);
    class_addmethod(retroseq_class, (t_method)retroseq_trigger_sent, gensym("bang"), 0);
    class_addmethod(retroseq_class, (t_method)retroseq_play_backwards, gensym("play_backwards"), A_GIMME, 0);

    /* Print message to Max window */
    post("retroseq~ • External was loaded");
}

/* The 'new instance' routine *************************************************/
void *retroseq_new(t_symbol *s, short argc, t_atom *argv)
{
    /* Instantiate a new object */
    t_retroseq *x = (t_retroseq *)pd_new(retroseq_class);

    return retroseq_common_new(x, argc, argv);
}

/******************************************************************************/

/* The argument parsing utility functions *************************************/
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

    if (argc > arg_index) { *variable = atom_getsymbolarg(arg_index, argc, argv); }

}

/* The 'new' and 'delete' pointers utility functions **************************/
void *new_memory(long nbytes)
{
    void *pointer = getbytes(nbytes);

    if (pointer == NULL) {
        pd_error(NULL, "retroseq~ • Cannot allocate memory for this object");
        return NULL;
    }
    return pointer;
}

void free_memory(void *ptr, long nbytes)
{
    freebytes(ptr, nbytes);
}

/* The common 'new instance' routine ******************************************/
void *retroseq_common_new(t_retroseq *x, short argc, t_atom *argv)
{
    /* Create inlets */
    //nothing

    /* Create signal outlets */
    outlet_new(&x->obj, gensym("signal"));

    /* Create non-signal outlets */
    x->adsr_outlet = outlet_new(&x->obj, gensym("list"));
    x->bang_outlet = outlet_new(&x->obj, gensym("bang"));
    x->shuffle_freqs_outlet = outlet_new(&x->obj, gensym("list"));
    x->shuffle_durs_outlet = outlet_new(&x->obj, gensym("list"));

    /* Initialize clocks */
    x->bang_clock = clock_new(x, (t_method)retroseq_send_bang);

    /* Parse passed arguments */
    //nothing

    /* Initialize some state variables */
    x->fs = sys_getsr();

    x->max_sequence_bytes = MAXIMUM_SEQUENCE_LENGTH * sizeof(float);

    x->note_sequence = (float *)new_memory(x->max_sequence_bytes);
    x->note_sequence_length = DEFAULT_SEQUENCE_LENGTH;
    x->note_sequence[0] = F0;
    x->note_sequence[1] = F1;
    x->note_sequence[2] = F2;

    x->duration_sequence = (float *)new_memory(x->max_sequence_bytes);
    x->duration_sequence_length = DEFAULT_SEQUENCE_LENGTH;
    x->duration_sequence[0] = D0;
    x->duration_sequence[1] = D1;
    x->duration_sequence[2] = D2;

    srand((unsigned int)clock());
    x->shuffle_list = (t_atom *)new_memory(MAXIMUM_SEQUENCE_LENGTH * sizeof(t_atom));

    x->tempo_bpm = DEFAULT_TEMPO_BPM;
    x->elastic_sustain = 0;
    x->sustain_amplitude = DEFAULT_SUSTAIN_AMPLITUDE;

    x->adsr_bytes = 4 * sizeof(float);
    x->adsr = (float *)new_memory(x->adsr_bytes);
    x->adsr[0] = DEFAULT_ATACK_DURATION;
    x->adsr[1] = DEFAULT_DECAY_DURATION;
    x->adsr[2] = DEFAULT_SUSTAIN_DURATION;
    x->adsr[3] = DEFAULT_RELEASE_DURATION;

    x->adsr_out_bytes = 10 * sizeof(float);
    x->adsr_out = (float *)new_memory(x->adsr_out_bytes);
    x->adsr_list_bytes = 10 * sizeof(t_atom);
    x->adsr_list = (t_atom *)new_memory(x->adsr_list_bytes);

    /* Print message to Max window */
    post("retroseq~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine ************************************************/
void retroseq_free(t_retroseq *x)
{

    /* Free allocated dynamic memory */
    clock_free(x->bang_clock);
    clock_free(x->adsr_clock);

    free_memory(x->note_sequence, x->max_sequence_bytes);
    free_memory(x->duration_sequence, x->max_sequence_bytes);

    free_memory(x->shuffle_list, MAXIMUM_SEQUENCE_LENGTH * sizeof(t_atom));

    free_memory(x->adsr, x->adsr_bytes);
    free_memory(x->adsr_out, x->adsr_out_bytes);
    free_memory(x->adsr_list, x->adsr_list_bytes);

    /* Print message to Max window */
    post("retroseq~ • Memory was freed");
}

/* The object-specific methods ************************************************/
void send_sequence_as_list(int the_length,
                           float *the_sequence,
                           t_atom *the_list,
                           void *the_outlet)
{
    for (int ii = 0; ii < the_length; ii++) {
        SETFLOAT(the_list + ii, the_sequence[ii]);
    }
    outlet_list(the_outlet, NULL, the_length, the_list);
}

void retroseq_list(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv)
{
    if (argc < 2) {
        pd_error(x, "retroseq~ • The sequence must have at least two members");
        return;
    }
    if (argc % 2) {
        pd_error(x, "retroseq~ • The sequence must have odd number of members");
        return;
    }

    if (argc > 2 * MAXIMUM_SEQUENCE_LENGTH) {
        argc = 2 * MAXIMUM_SEQUENCE_LENGTH;
    }

    for (int ii = 0, jj = 0; jj < argc; ii++, jj += 2) {
        x->note_sequence[ii] = atom_getfloat(argv + jj);
        x->duration_sequence[ii] = atom_getfloat(argv + jj+1);
    }

    x->note_sequence_length = argc / 2;
    x->note_counter = x->note_sequence_length - 1;
    x->duration_sequence_length = argc / 2;
    x->duration_counter = x->duration_sequence_length - 1;

    send_sequence_as_list(x->note_sequence_length,
                          x->note_sequence,
                          x->shuffle_list,
                          x->shuffle_freqs_outlet);

    send_sequence_as_list(x->duration_sequence_length,
                          x->duration_sequence,
                          x->shuffle_list,
                          x->shuffle_durs_outlet);
}

void retroseq_freqlist(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv)
{
    if (argc < 2) {
        pd_error(x, "retroseq~ • The sequence must have at least two members");
        return;
    }

    if (argc > MAXIMUM_SEQUENCE_LENGTH) {
        argc = MAXIMUM_SEQUENCE_LENGTH;
    }

    for (int ii = 0; ii < argc; ii++) {
        x->note_sequence[ii] = atom_getfloat(argv + ii);
    }

    x->note_sequence_length = argc;
    x->note_counter = x->note_sequence_length - 1;

    send_sequence_as_list(x->note_sequence_length,
                          x->note_sequence,
                          x->shuffle_list,
                          x->shuffle_freqs_outlet);
}

void retroseq_durlist(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv)
{
    if (argc < 2) {
        pd_error(x, "retroseq~ • The sequence must have at least two members");
        return;
    }

    if (argc > MAXIMUM_SEQUENCE_LENGTH) {
        argc = MAXIMUM_SEQUENCE_LENGTH;
    }

    for (int ii = 0; ii < argc; ii++) {
        x->duration_sequence[ii] = atom_getfloat(argv + ii);
    }

    x->duration_sequence_length = argc;
    x->duration_counter = x->duration_sequence_length - 1;

    send_sequence_as_list(x->duration_sequence_length,
                          x->duration_sequence,
                          x->shuffle_list,
                          x->shuffle_durs_outlet);
}

void retroseq_permute(float *sequence, int length)
{
    while (length > 0) {
        int random_position = rand() % length;

        float temp = sequence[random_position];
        sequence[random_position] = sequence[length - 1];
        sequence[length - 1] = temp;

        length--;
    }
}

void retroseq_shuffle_freqs(t_retroseq *x)
{
    retroseq_permute(x->note_sequence, x->note_sequence_length);

    send_sequence_as_list(x->note_sequence_length,
                          x->note_sequence,
                          x->shuffle_list,
                          x->shuffle_freqs_outlet);
}

void retroseq_shuffle_durs(t_retroseq *x)
{
    retroseq_permute(x->duration_sequence, x->duration_sequence_length);

    send_sequence_as_list(x->duration_sequence_length,
                          x->duration_sequence,
                          x->shuffle_list,
                          x->shuffle_durs_outlet);
}

void retroseq_shuffle(t_retroseq *x)
{
    retroseq_shuffle_freqs(x);
    retroseq_shuffle_durs(x);
}

void retroseq_set_tempo(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv)
{
    float new_tempo_bpm;
    if (argc == 1) {
        new_tempo_bpm = atom_getfloat(argv);
    } else {
        return;
    }

    if (new_tempo_bpm <= 0) {
        pd_error(x, "retroseq~ • Tempo must be greater than zero");
        return;
    }

    float old_tempo_bpm = x->tempo_bpm;
    x->tempo_bpm = new_tempo_bpm;

    x->duration_factor = (60.0 / new_tempo_bpm) * (x->fs / 1000.0);
    x->sample_counter *= old_tempo_bpm / new_tempo_bpm;
}

void retroseq_set_elastic_sustain(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv)
{
    if (argc == 1) {
        x->elastic_sustain = atom_getfloat(argv);
    }
}

void retroseq_set_sustain_amplitude(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv)
{
    if (argc == 1) {
        short state = (short)atom_getfloat(argv);
        x->sustain_amplitude = state;
    }

    if (x->sustain_amplitude < 0) {
        x->sustain_amplitude = 0;
    } else if (x->sustain_amplitude > 1) {
        x->sustain_amplitude = 1;
    }
}

void retroseq_set_adsr(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv)
{
    if (argc != 4) {
        pd_error(x, "retroseq~ • The envelope must have four members");
        return;
    }

    for (int ii = 0; ii < argc; ii++) {
        x->adsr[ii] = atom_getfloat(argv + ii);
        if (x->adsr[ii] < 1.0) {
            x->adsr[ii] = 1.0;
        }
    }
}

void retroseq_send_adsr(t_retroseq *x)
{
    short elastic_sustain = x->elastic_sustain;

    float *adsr = x->adsr;
    float *adsr_out = x->adsr_out;
    t_atom *adsr_list = x->adsr_list;

    float note_duration_ms = x->duration_sequence[x->duration_counter]
                             * (60.0 / x->tempo_bpm);

    adsr_out[0] = 0.0;
    adsr_out[1] = 0.0;
    adsr_out[2] = 1.0;
    adsr_out[3] = adsr[0];
    adsr_out[4] = x->sustain_amplitude;
    adsr_out[5] = adsr[1];
    adsr_out[6] = x->sustain_amplitude;

    adsr_out[8] = 0.0;
    adsr_out[9] = adsr[3];

    if (x->manual_override) {
        adsr_out[7] = adsr[2];
    } else {
        if (elastic_sustain) {
            adsr_out[7] = note_duration_ms - (adsr[0] + adsr[1] + adsr[3]);
            if (adsr_out[7] < 1.0) {
                adsr_out[7] = 1.0;
            }
        } else {
            adsr_out[7] = adsr[2];
        }

        float duration_sum = adsr_out[3] + adsr_out[5] + adsr_out[7] + adsr_out[9];
        if (duration_sum > note_duration_ms) {
            float rescale = note_duration_ms / duration_sum;
            adsr_out[3] *= rescale;
            adsr_out[5] *= rescale;
            adsr_out[7] *= rescale;
            adsr_out[9] *= rescale;
        }
    }

    for (int ii = 0; ii < 10; ii++) {
        SETFLOAT(adsr_list + ii, adsr_out[ii]);
    }
    outlet_list(x->adsr_outlet, NULL, 10, adsr_list);
}

void retroseq_send_bang(t_retroseq *x)
{
    outlet_bang(x->bang_outlet);
}

void retroseq_manual_override(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv)
{
    if (argc == 1) {
        short state = (short)atom_getfloat(argv);
        x->manual_override = state;
    }
}

void retroseq_trigger_sent(t_retroseq *x)
{
    x->trigger_sent = 1;
}

void retroseq_play_backwards(t_retroseq *x, t_symbol *msg, short argc, t_atom *argv)
{
    if (argc == 1) {
        short state = (short)atom_getfloat(argv);

        if (x->play_backwards != state) {
            x->play_backwards = (short)state;

            float *sequence;
            int length;
            int position;

            sequence = x->note_sequence;
            length = x->note_sequence_length;
            position = 0;
            while (position < length) {
                float temp = sequence[position];
                sequence[position] = sequence[length - 1];
                sequence[length - 1] = temp;

                position++;
                length--;
            }
            send_sequence_as_list(x->note_sequence_length,
                                  x->note_sequence,
                                  x->shuffle_list,
                                  x->shuffle_freqs_outlet);

            sequence = x->duration_sequence;
            length = x->duration_sequence_length;
            position = 0;
            while (position < length) {
                float temp = sequence[position];
                sequence[position] = sequence[length - 1];
                sequence[length - 1] = temp;

                position++;
                length--;
            }
            send_sequence_as_list(x->duration_sequence_length,
                                  x->duration_sequence,
                                  x->shuffle_list,
                                  x->shuffle_durs_outlet);
        }
    }
}

/******************************************************************************/

/* The 'DSP' method ***********************************************************/

void retroseq_dsp(t_retroseq *x, t_signal **sp, short *count)
{
    /* Initialize the remaining state variables */
    x->duration_factor = (60.0 / x->tempo_bpm) * (x->fs / 1000.0);
    x->sample_counter = 0;

    x->current_note_value = x->note_sequence[0];
    x->note_counter = x->note_sequence_length - 1;

    x->current_duration_value = x->duration_sequence[0];
    x->duration_counter = x->duration_sequence_length - 1;

    x->manual_override = 0;
    x->play_backwards = 0;

    /* Adjust to changes in the sampling rate */
    if (x->fs != sp[0]->s_sr) {
        x->duration_factor *= sp[0]->s_sr / x->fs;
        x->sample_counter *= x->fs / sp[0]->s_sr;

        x->fs = sp[0]->s_sr;
    }

    /* Attach the object to the DSP chain */
    dsp_add(retroseq_perform, NEXT-1, x,
            sp[0]->s_vec,
            sp[0]->s_n);

    /* Print message to Max window */
    post("retroseq~ • Executing 32-bit perform routine");
}

/* The 'perform' routine ******************************************************/
t_int *retroseq_perform(t_int *w)
{
    /* Copy the object pointer */
    t_retroseq *x = (t_retroseq *)w[OBJECT];

    /* Copy signal pointers */
    t_float *output = (t_float *)w[OUTPUT];

    /* Copy the signal vector size */
    t_int n = w[VECTOR_SIZE];

    /* Load state variables */
    float *note_sequence = x->note_sequence;
    int note_sequence_length = x->note_sequence_length;
    float current_note_value = x->current_note_value;
    int note_counter = x->note_counter;

    float *duration_sequence = x->duration_sequence;
    int duration_sequence_length = x->duration_sequence_length;
    float current_duration_value = x->current_duration_value;
    int duration_counter = x->duration_counter;

    float duration_factor = x->duration_factor;
    int sample_counter = x->sample_counter;

    short manual_override = x->manual_override;
    short trigger_sent = x->trigger_sent;

    /* Perform the DSP loop */
    if (manual_override) {
        while (n--)
        {
            if (trigger_sent) {
                trigger_sent = 0;

                if (++note_counter >= note_sequence_length) {
                    note_counter = 0;
                    clock_delay(x->bang_clock, 0);
                }

                current_note_value = note_sequence[note_counter];
                clock_delay(x->adsr_clock, 0);
            }
            
            *output++ = current_note_value;
        }
    }

    else {
        while (n--)
        {
            if (sample_counter-- <= 0) {
                if (++note_counter >= note_sequence_length) {
                    note_counter = 0;
                    clock_delay(x->bang_clock, 0);
                }

                if (++duration_counter >= duration_sequence_length) {
                    duration_counter = 0;
                }

                current_duration_value = duration_sequence[duration_counter];
                sample_counter = current_duration_value * duration_factor;

                current_note_value = note_sequence[note_counter];
                clock_delay(x->adsr_clock, 0);
            }
            
            *output++ = current_note_value;
        }
    }

    /* Update state variables */
    x->current_note_value = current_note_value;
    x->note_counter = note_counter;

    x->current_duration_value = current_duration_value;
    x->duration_counter = duration_counter;
    
    x->sample_counter = sample_counter;
    x->trigger_sent = trigger_sent;
    
    /* Return the next address in the DSP chain */
    return w + NEXT;
}
