#include "mscHack.hpp"

#define nCHANNELS 4

#define MID_INDEX 12
#define CLOCK_DIVS 25
const int multdisplayval[CLOCK_DIVS] = {32, 24, 16, 12, 9, 8, 7, 6, 5,  4,  3,  2, 1,
                                        2,  3,  4,  5,  6, 7, 8, 9, 12, 16, 24, 32};

//-----------------------------------------------------
// Module Definition
//
//-----------------------------------------------------
struct MasterClockx4 : Module
{
    enum ParamIds
    {
        PARAM_BPM,
        PARAM_MULT,
        PARAM_HUMANIZE = PARAM_MULT + nCHANNELS,
        nPARAMS
    };

    enum InputIds
    {
        INPUT_EXT_SYNC,
        INPUT_CHAIN = INPUT_EXT_SYNC + nCHANNELS,
        nINPUTS
    };

    enum OutputIds
    {
        OUTPUT_CLK,
        OUTPUT_TRIG = OUTPUT_CLK + (nCHANNELS * 4),
        OUTPUT_CHAIN = OUTPUT_TRIG + (nCHANNELS * 4),
        nOUTPUTS
    };

    float m_fBPM = 120;

    std::atomic<bool> m_refreshWidgets{false};

    bool m_bGlobalSync = false;
    bool m_bStopState[nCHANNELS] = {};
    bool m_bGlobalStopState = false;
    bool m_bChannelSyncPending[nCHANNELS] = {};
    bool m_bTimeX2[nCHANNELS] = {};

    int m_bIsTriggered[nCHANNELS]{};

    int m_ChannelDivBeatCount[nCHANNELS] = {};
    float m_fChannelBeatsPers[nCHANNELS] = {};
    float m_fChannelClockCount[nCHANNELS] = {};
    int m_ChannelMultSelect[nCHANNELS] = {};

    float m_fHumanize = 0;
    float m_bWasChained = false;

    float m_fBeatsPers;
    float m_fMainClockCount;

    dsp::PulseGenerator m_PulseClock[nCHANNELS];
    dsp::PulseGenerator m_PulseSync[nCHANNELS];
    dsp::SchmittTrigger m_SchmittSyncIn[nCHANNELS];

    // Contructor
    MasterClockx4()
    {
        config(nPARAMS, nINPUTS, nOUTPUTS);

        configParam(PARAM_BPM, 60.0, 220.0, 120.0, "Beats Per Minute");

        for (int i = 0; i < nCHANNELS; i++)
        {
            configParam(PARAM_MULT + i, 0.0f, (float)(CLOCK_DIVS - 1), (float)(MID_INDEX),
                        "Clock Mult/Div");
        }

        configParam(PARAM_HUMANIZE, 0.0f, 1.0f, 0.0f, "Humanize clock");

        configInput(INPUT_CHAIN, "Chain");
        for (int i = 0; i < nCHANNELS; ++i)
        {
            auto s = std::to_string(i + 1);
            configInput(INPUT_EXT_SYNC + i, "Sync " + s);
        }

        for (int i = 0; i < nCHANNELS; ++i)
        {
            auto s = std::to_string(i + 1);
            for (int c = 0; c < 4; ++c)
            {
                auto cs = std::string();
                configOutput(OUTPUT_CLK + (i * 4) + c, "Clock " + s + cs);
                configOutput(OUTPUT_TRIG + (i * 4) + c, "Trig " + s + cs);
            }
        }
        configOutput(OUTPUT_CHAIN, "Chain");
    }

    //-----------------------------------------------------
    // MyHumanize_Knob
    //-----------------------------------------------------
    struct MyHumanize_Knob : MSCH_Widget_Knob1
    {
        MyHumanize_Knob() { set(DWRGB(188, 175, 167), DWRGB(0, 0, 0), 13.0f); }

        void onChange(const event::Change &e) override
        {
            MasterClockx4 *mymodule;

            MSCH_Widget_Knob1::onChange(e);
            ParamQuantity *paramQuantity = getParamQuantity();
            mymodule = (MasterClockx4 *)paramQuantity->module;

            if (!mymodule)
                return;

            if (!mymodule->inputs[INPUT_CHAIN].isConnected())
                mymodule->GetNewHumanizeVal();
        }
    };

    //-----------------------------------------------------
    // MyBPM_Knob
    //-----------------------------------------------------
    struct MyBPM_Knob : MSCH_Widget_Knob1
    {
        MyBPM_Knob() { set(DWRGB(188, 175, 167), DWRGB(0, 0, 0), 28.0f); }

        void onChange(const event::Change &e) override
        {
            MasterClockx4 *mymodule;

            MSCH_Widget_Knob1::onChange(e);
            ParamQuantity *paramQuantity = getParamQuantity();
            mymodule = (MasterClockx4 *)paramQuantity->module;

            if (!mymodule)
                return;

            if (!mymodule->inputs[INPUT_CHAIN].isConnected())
                mymodule->BPMChange(paramQuantity->getValue(), false);
        }
    };

    //-----------------------------------------------------
    // MyMult_Knob
    //-----------------------------------------------------
    struct MyMult_Knob : MSCH_Widget_Knob1
    {
        MyMult_Knob()
        {
            snap = true;
            set(DWRGB(188, 175, 167), DWRGB(0, 0, 0), 13.0f);
        }

        void onChange(const event::Change &e) override
        {
            MasterClockx4 *mymodule;
            int ch;

            MSCH_Widget_Knob1::onChange(e);
            ParamQuantity *paramQuantity = getParamQuantity();
            mymodule = (MasterClockx4 *)paramQuantity->module;

            if (!mymodule)
                return;

            ch = paramQuantity->paramId - MasterClockx4::PARAM_MULT;
            mymodule->SetDisplayLED(ch, (int)paramQuantity->getValue());
        }
    };

    // Overrides
    void JsonParams(bool bTo, json_t *root);
    void process(const ProcessArgs &args) override;
    json_t *dataToJson() override;
    void dataFromJson(json_t *rootJ) override;
    void onReset() override;

    void GetNewHumanizeVal(void);
    void BPMChange(float fbmp, bool bforce);
    void CalcChannelClockRate(int ch);
    void SetDisplayLED(int ch, int val);
};

//-----------------------------------------------------
// MyLEDButton_GlobalStop
//-----------------------------------------------------
void MyLEDButton_GlobalStop(void *pClass, int id, bool bOn)
{
    MasterClockx4 *mymodule;
    mymodule = (MasterClockx4 *)pClass;
    mymodule->m_bGlobalStopState = bOn;
}

//-----------------------------------------------------
// MyLEDButton_TimeX2
//-----------------------------------------------------
void MyLEDButton_TimeX2(void *pClass, int id, bool bOn)
{
    MasterClockx4 *mymodule;
    mymodule = (MasterClockx4 *)pClass;
    mymodule->m_bTimeX2[id] = bOn;
    mymodule->SetDisplayLED(id, (int)mymodule->params[MasterClockx4::PARAM_MULT + id].getValue());
}

//-----------------------------------------------------
// MyLEDButton_GlobalTrig
//-----------------------------------------------------
void MyLEDButton_GlobalTrig(void *pClass, int id, bool bOn)
{
    MasterClockx4 *mymodule;
    mymodule = (MasterClockx4 *)pClass;
    mymodule->m_bGlobalSync = true;
}

//-----------------------------------------------------
// MyLEDButton_ChannelStop
//-----------------------------------------------------
void MyLEDButton_ChannelStop(void *pClass, int id, bool bOn)
{
    MasterClockx4 *mymodule;
    mymodule = (MasterClockx4 *)pClass;
    mymodule->m_bStopState[id] = bOn;
}

//-----------------------------------------------------
// MyLEDButton_ChannelSync
//-----------------------------------------------------
void MyLEDButton_ChannelSync(void *pClass, int id, bool bOn)
{
    MasterClockx4 *mymodule;
    mymodule = (MasterClockx4 *)pClass;
    mymodule->m_bChannelSyncPending[id] = true;

    mymodule->m_bIsTriggered[id] = 5;
}

//-----------------------------------------------------
// Procedure:   Widget
//
//-----------------------------------------------------

struct MasterClockx4_Widget : ModuleWidget
{
    MyLED7DigitDisplay *m_pDigitDisplayMult[nCHANNELS] = {};
    MyLED7DigitDisplay *m_pDigitDisplayBPM = NULL;

    MyLEDButton *m_pButtonGlobalStop = NULL;
    MyLEDButton *m_pButtonGlobalTrig = NULL;
    MyLEDButton *m_pButtonStop[nCHANNELS] = {};
    MyLEDButton *m_pButtonTrig[nCHANNELS] = {};
    MyLEDButton *m_pButtonTimeX2[nCHANNELS] = {};

    ParamWidget *m_pBpmKnob = NULL;
    ParamWidget *m_pHumanKnob = NULL;

    MasterClockx4_Widget(MasterClockx4 *module)
    {
        int ch, x, y;

        setModule(module);

        setPanel(APP->window->loadSvg(asset::plugin(thePlugin, "res/MasterClockx4.svg")));

        addChild(createWidget<ScrewSilver>(Vec(15, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
        addChild(createWidget<ScrewSilver>(Vec(15, 365)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

        // bpm knob
        m_pBpmKnob =
            createParam<MasterClockx4::MyBPM_Knob>(Vec(9, 50), module, MasterClockx4::PARAM_BPM);
        addParam(m_pBpmKnob);

        // bpm display
        m_pDigitDisplayBPM =
            new MyLED7DigitDisplay(5, 115, 0.055, DWRGB(0, 0, 0), DWRGB(0xFF, 0xFF, 0xFF),
                                   MyLED7DigitDisplay::TYPE_FLOAT2, 5);
        addChild(m_pDigitDisplayBPM);

        // global stop switch
        m_pButtonGlobalStop =
            new MyLEDButton(22, 144, 25, 25, 20.0, DWRGB(180, 180, 180), DWRGB(255, 0, 0),
                            MyLEDButton::TYPE_SWITCH, 0, module, MyLEDButton_GlobalStop);
        addChild(m_pButtonGlobalStop);

        // global sync button
        m_pButtonGlobalTrig =
            new MyLEDButton(22, 202, 25, 25, 20.0, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
                            MyLEDButton::TYPE_MOMENTARY, 0, module, MyLEDButton_GlobalTrig);
        addChild(m_pButtonGlobalTrig);

        // humanize knob
        m_pHumanKnob = createParam<MasterClockx4::MyHumanize_Knob>(Vec(22, 235), module,
                                                                   MasterClockx4::PARAM_HUMANIZE);
        addParam(m_pHumanKnob);

        // add chain out
        addOutput(createOutput<MyPortOutSmall>(Vec(30, 345), module, MasterClockx4::OUTPUT_CHAIN));

        // chain in
        addInput(createInput<MyPortInSmall>(Vec(30, 13), module, MasterClockx4::INPUT_CHAIN));

        x = 91;
        y = 39;

        for (ch = 0; ch < nCHANNELS; ch++)
        {
            // x2
            m_pButtonTimeX2[ch] =
                new MyLEDButton(x + 2, y + 2, 11, 11, 9.0, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
                                MyLEDButton::TYPE_SWITCH, ch, module, MyLEDButton_TimeX2);
            addChild(m_pButtonTimeX2[ch]);

            // clock mult knob
            addParam(createParam<MasterClockx4::MyMult_Knob>(Vec(x + 13, y + 13), module,
                                                             MasterClockx4::PARAM_MULT + ch));

            // mult display
            m_pDigitDisplayMult[ch] =
                new MyLED7DigitDisplay(x + 10, y + 48, 0.07, DWRGB(0, 0, 0),
                                       DWRGB(0xFF, 0xFF, 0xFF), MyLED7DigitDisplay::TYPE_INT, 2);
            addChild(m_pDigitDisplayMult[ch]);

            // sync triggers
            m_pButtonTrig[ch] = new MyLEDButton(x + 76, y + 4, 19, 19, 15.0, DWRGB(180, 180, 180),
                                                DWRGB(0, 255, 255), MyLEDButton::TYPE_MOMENTARY, ch,
                                                module, MyLEDButton_ChannelSync);
            addChild(m_pButtonTrig[ch]);

            addInput(createInput<MyPortInSmall>(Vec(x + 54, y + 5), module,
                                                MasterClockx4::INPUT_EXT_SYNC + ch));
            addOutput(createOutput<MyPortOutSmall>(Vec(x + 54, y + 31), module,
                                                   MasterClockx4::OUTPUT_TRIG + (ch * 4) + 0));
            addOutput(createOutput<MyPortOutSmall>(Vec(x + 54, y + 53), module,
                                                   MasterClockx4::OUTPUT_TRIG + (ch * 4) + 1));
            addOutput(createOutput<MyPortOutSmall>(Vec(x + 77, y + 31), module,
                                                   MasterClockx4::OUTPUT_TRIG + (ch * 4) + 2));
            addOutput(createOutput<MyPortOutSmall>(Vec(x + 77, y + 53), module,
                                                   MasterClockx4::OUTPUT_TRIG + (ch * 4) + 3));

            // clock out
            m_pButtonStop[ch] = new MyLEDButton(x + 132, y + 4, 19, 19, 15.0, DWRGB(180, 180, 180),
                                                DWRGB(255, 0, 0), MyLEDButton::TYPE_SWITCH, ch,
                                                module, MyLEDButton_ChannelStop);
            addChild(m_pButtonStop[ch]);

            addOutput(createOutput<MyPortOutSmall>(Vec(x + 107, y + 31), module,
                                                   MasterClockx4::OUTPUT_CLK + (ch * 4) + 0));
            addOutput(createOutput<MyPortOutSmall>(Vec(x + 107, y + 53), module,
                                                   MasterClockx4::OUTPUT_CLK + (ch * 4) + 1));
            addOutput(createOutput<MyPortOutSmall>(Vec(x + 130, y + 31), module,
                                                   MasterClockx4::OUTPUT_CLK + (ch * 4) + 2));
            addOutput(createOutput<MyPortOutSmall>(Vec(x + 130, y + 53), module,
                                                   MasterClockx4::OUTPUT_CLK + (ch * 4) + 3));

            y += 80;
        }

        if (module)
        {
            module->m_refreshWidgets = true;
        }
    }

    void step() override
    {
        auto az = dynamic_cast<MasterClockx4 *>(module);
        if (az)
        {
            if (az->m_refreshWidgets)
            {
                az->m_refreshWidgets = false;
                m_pButtonGlobalStop->Set(az->m_bGlobalStopState);

                for (int ch = 0; ch < nCHANNELS; ch++)
                {
                    m_pButtonStop[ch]->Set(az->m_bStopState[ch]);
                    m_pButtonTimeX2[ch]->Set(az->m_bTimeX2[ch]);
                    // lg.f( "value = %d\n", (int)params[ PARAM_MULT + ch ].value );
                    az->SetDisplayLED(ch, az->m_ChannelMultSelect[ch]);
                }

                // These two are pretty dicey
                az->m_fMainClockCount = 0;
                az->BPMChange(az->params[MasterClockx4::PARAM_BPM].value, true);

                if (m_pDigitDisplayBPM)
                    m_pDigitDisplayBPM->SetVal((int)(az->m_fBPM * 100.0));
            }

            m_pButtonGlobalStop->Set(az->m_bGlobalStopState);

            if (az->m_bWasChained == m_pHumanKnob->visible)
            {
                m_pHumanKnob->visible = !az->m_bWasChained;
                m_pBpmKnob->visible = !az->m_bWasChained;
            }

            for (int i = 0; i < nCHANNELS; ++i)
            {
                if (az->m_bIsTriggered[i] > 1)
                {
                    az->m_bIsTriggered[i]--;
                    m_pButtonTrig[i]->Set(true);
                }
                if (az->m_bIsTriggered[i] == 1)
                {
                    az->m_bIsTriggered[i]--;
                    m_pButtonTrig[i]->Set(false);
                }
            }

            m_pDigitDisplayBPM->SetVal((int)(az->m_fBPM * 100.0));
            for (int i = 0; i < nCHANNELS; ++i)
            {
                stepDisplayLED(az, i);
            }
        }
        Widget::step();
    }

    void stepDisplayLED(MasterClockx4 *az, int ch)
    {
        auto val = az->m_ChannelMultSelect[ch];
        int col, mult = 1;

        if (az->m_bTimeX2[ch])
            mult = 2;

        if (val < MID_INDEX)
        {
            col = DWRGB(0xFF, 0, 0);
        }
        else if (val > MID_INDEX)
        {
            col = DWRGB(0, 0xFF, 0xFF);
        }
        else
        {
            mult = 1;
            col = DWRGB(0xFF, 0xFF, 0xFF);
        }

        auto sv = multdisplayval[val] * mult;
        if (m_pDigitDisplayMult[ch]->m_iVal != sv)
        {
            m_pDigitDisplayMult[ch]->SetLEDCol(col);
            m_pDigitDisplayMult[ch]->SetVal(multdisplayval[val] * mult);
        }
    }
};

//-----------------------------------------------------
// Procedure: JsonParams
//
//-----------------------------------------------------
void MasterClockx4::JsonParams(bool bTo, json_t *root)
{
    JsonDataBool(bTo, "m_bGlobalStopState", root, &m_bGlobalStopState, 1);
    JsonDataBool(bTo, "m_bStopState", root, m_bStopState, nCHANNELS);
    JsonDataBool(bTo, "m_bTimeX2", root, m_bTimeX2, nCHANNELS);
    JsonDataInt(bTo, "m_ChannelMultSelect", root, m_ChannelMultSelect, nCHANNELS);
}

//-----------------------------------------------------
// Procedure:
//
//-----------------------------------------------------
json_t *MasterClockx4::dataToJson()
{

    json_t *root = json_object();

    if (!root)
        return NULL;

    JsonParams(TOJSON, root);

    return root;
}

//-----------------------------------------------------
// Procedure:   fromJson
//
//-----------------------------------------------------
void MasterClockx4::dataFromJson(json_t *root)
{
    JsonParams(FROMJSON, root);

    m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   reset
//
//-----------------------------------------------------
void MasterClockx4::onReset()
{
    m_fBPM = 120;

    m_bGlobalStopState = false;
    m_refreshWidgets = true;

    for (int ch = 0; ch < nCHANNELS; ch++)
    {
        m_bTimeX2[ch] = false;
        m_bStopState[ch] = false;
        SetDisplayLED(ch, MID_INDEX);
    }

    BPMChange(m_fBPM, true);
}

//-----------------------------------------------------
// Procedure:   SetDisplayLED
//
//-----------------------------------------------------
void MasterClockx4::SetDisplayLED(int ch, int val)
{
    m_ChannelMultSelect[ch] = val;
    CalcChannelClockRate(ch);
}

//-----------------------------------------------------
// Procedure:   GetNewHumanizeVal
//
//-----------------------------------------------------
void MasterClockx4::GetNewHumanizeVal(void)
{
    m_fHumanize =
        random::uniform() * APP->engine->getSampleRate() * 0.1 * params[PARAM_HUMANIZE].getValue();

    if (random::uniform() > 0.5)
        m_fHumanize *= -1;
}

//-----------------------------------------------------
// Procedure:   BMPChange
//
//-----------------------------------------------------
void MasterClockx4::BPMChange(float fbpm, bool bforce)
{
    // don't change if it is already the same
    if (!bforce && ((int)(fbpm * 1000.0f) == (int)(m_fBPM * 1000.0f)))
        return;

    m_fBPM = fbpm;
    m_fBeatsPers = fbpm / 60.0;

    for (int i = 0; i < nCHANNELS; i++)
        CalcChannelClockRate(i);
}

//-----------------------------------------------------
// Procedure:   CalcChannelClockRate
//
//-----------------------------------------------------
void MasterClockx4::CalcChannelClockRate(int ch)
{
    int mult = 1;

    if (m_bTimeX2[ch])
        mult = 2;

    // for beat division just keep a count of beats
    if (m_ChannelMultSelect[ch] == MID_INDEX)
        m_ChannelDivBeatCount[ch] = 1;
    else if (m_ChannelMultSelect[ch] < MID_INDEX)
        m_ChannelDivBeatCount[ch] = multdisplayval[m_ChannelMultSelect[ch]] * mult;
    else
        m_fChannelBeatsPers[ch] =
            m_fBeatsPers * (float)(multdisplayval[m_ChannelMultSelect[ch]] * mult);
}

//-----------------------------------------------------
// Procedure:   step
//
//-----------------------------------------------------
void MasterClockx4::process(const ProcessArgs &args)
{
    int ch, mult = 1;
    unsigned int ival;
    float fSyncPulseOut, fClkPulseOut;
    bool bMainClockTrig = false, bChannelClockTrig;

    // use the input chain trigger for our clock
    if (inputs[INPUT_CHAIN].isConnected())
    {
        m_bWasChained = true;

        // value of less than zero is a trig
        if (inputs[INPUT_CHAIN].value < 8.0)
        {
            ival = (unsigned int)inputs[INPUT_CHAIN].getVoltage();

            bMainClockTrig = (ival & 1);

            m_bGlobalSync = (ival & 2);

            m_bGlobalStopState = (ival & 4);
        }
        // values greater than zero are the bpm
        else
        {
            BPMChange(inputs[INPUT_CHAIN].getVoltage(), false);
        }
    }
    else
    {
        // go back to our bpm if chain removed
        if (m_bWasChained)
        {
            m_bWasChained = false;
            BPMChange(params[PARAM_BPM].value, false);
        }

        // keep track of main bpm
        m_fMainClockCount += m_fBeatsPers;
        // if( ( m_fMainClockCount + m_fHumanize ) >= engineGetSampleRate() )
        if (m_fMainClockCount >= (APP->engine->getSampleRate() - 1))
        {
            // m_fMainClockCount = ( m_fMainClockCount + m_fHumanize ) - engineGetSampleRate();
            m_fMainClockCount = m_fMainClockCount - (APP->engine->getSampleRate() - 1);

            GetNewHumanizeVal();

            bMainClockTrig = true;
        }
    }

    // send chain
    if (outputs[OUTPUT_CHAIN].isConnected())
    {
        ival = 0;

        if (bMainClockTrig)
            ival |= 1;

        if (m_bGlobalSync)
            ival |= 2;

        if (m_bGlobalStopState)
            ival |= 4;

        if (ival)
            outputs[OUTPUT_CHAIN].value = (float)ival;
        else
            outputs[OUTPUT_CHAIN].value = m_fBPM;
    }

    for (ch = 0; ch < nCHANNELS; ch++)
    {
        bChannelClockTrig = false;

        // sync triggers
        if (m_bGlobalSync || m_SchmittSyncIn[ch].process(inputs[INPUT_EXT_SYNC + ch].getVoltage()))
        {
            m_bIsTriggered[ch] = 5;

            m_bChannelSyncPending[ch] = true;
        }

        // sync all triggers to main clock pulse
        if (bMainClockTrig)
        {
            if (m_bChannelSyncPending[ch])
            {
                m_bChannelSyncPending[ch] = false;
                m_PulseSync[ch].trigger(1e-3);

                m_ChannelDivBeatCount[ch] = 0;
                m_fChannelClockCount[ch] = 0;
                bChannelClockTrig = true;
            }
            else
            {
                // divisions of clock will count beats
                if (m_ChannelMultSelect[ch] < MID_INDEX)
                {
                    if (m_bTimeX2[ch])
                        mult = 2;

                    if (++m_ChannelDivBeatCount[ch] >=
                        (multdisplayval[m_ChannelMultSelect[ch]] * mult))
                    {
                        m_ChannelDivBeatCount[ch] = 0;
                        bChannelClockTrig = true;
                    }
                }
                // multiples of clock will sync with every beat
                else
                {
                    m_fChannelClockCount[ch] = 0;
                    bChannelClockTrig = true;
                }
            }
        }
        // do multiple clocks
        else if (m_ChannelMultSelect[ch] > MID_INDEX)
        {
            m_fChannelClockCount[ch] += m_fChannelBeatsPers[ch];
            if (m_fChannelClockCount[ch] >= APP->engine->getSampleRate())
            {
                m_fChannelClockCount[ch] = m_fChannelClockCount[ch] - APP->engine->getSampleRate();
                bChannelClockTrig = true;
            }
        }

        if (bChannelClockTrig)
            m_PulseClock[ch].trigger(0.001f);

        // syncs
        fSyncPulseOut =
            m_PulseSync[ch].process(1.0 / APP->engine->getSampleRate()) ? CV_MAX10 : 0.0;
        outputs[OUTPUT_TRIG + (ch * 4) + 0].setVoltage(fSyncPulseOut);
        outputs[OUTPUT_TRIG + (ch * 4) + 1].setVoltage(fSyncPulseOut);
        outputs[OUTPUT_TRIG + (ch * 4) + 2].setVoltage(fSyncPulseOut);
        outputs[OUTPUT_TRIG + (ch * 4) + 3].setVoltage(fSyncPulseOut);

        // clocks
        fClkPulseOut =
            m_PulseClock[ch].process(1.0 / APP->engine->getSampleRate()) ? CV_MAX10 : 0.0;

        if (!m_bGlobalStopState && !m_bStopState[ch])
        {
            outputs[OUTPUT_CLK + (ch * 4) + 0].setVoltage(fClkPulseOut);
            outputs[OUTPUT_CLK + (ch * 4) + 1].setVoltage(fClkPulseOut);
            outputs[OUTPUT_CLK + (ch * 4) + 2].setVoltage(fClkPulseOut);
            outputs[OUTPUT_CLK + (ch * 4) + 3].setVoltage(fClkPulseOut);
        }
    }

    m_bGlobalSync = false;
}

Model *modelMasterClockx4 = createModel<MasterClockx4, MasterClockx4_Widget>("MasterClockx4");
