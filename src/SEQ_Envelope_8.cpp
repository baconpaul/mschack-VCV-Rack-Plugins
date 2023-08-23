#include "mscHack.hpp"
#include <array>

/*
 * Note this 2.0 port is a little 'less' pure than some others in that we retain
 * widget items as members storing data and do a various delayed refresh action from
 * step. See comments below.
 */

//-----------------------------------------------------
// Module Definition
//
//-----------------------------------------------------
struct SEQ_Envelope_8 : Module
{
#define nCHANNELS MAX_ENVELOPE_CHANNELS
#define nWAVESETS 4
#define nTIMESETS 6

    enum ParamIds
    {
        PARAM_BAND,
        nPARAMS
    };

    enum InputIds
    {
        INPUT_CLK_RESET,
        INPUT_CLK,
        INPUT_GLOBAL_TRIG,
        INPUT_CH_HOLD,
        INPUT_CH_TRIG = INPUT_CH_HOLD + nCHANNELS,
        nINPUTS = INPUT_CH_TRIG + nCHANNELS
    };

    enum OutputIds
    {
        OUTPUT_CV,
        nOUTPUTS = OUTPUT_CV + nCHANNELS
    };

    enum LightIds
    {
        nLIGHTS
    };

    bool m_bInitialized = false;

    std::shared_ptr<Widget_EnvelopeEdit::EditData> m_EditData;

    // Contructor
    SEQ_Envelope_8()
    {
        m_EditData = std::make_shared<Widget_EnvelopeEdit::EditData>();

        config(nPARAMS, nINPUTS, nOUTPUTS, nLIGHTS);

        configParam(PARAM_BAND, 0.0, 0.8, 0.333, "Rubber Band Edit");

        configInput(INPUT_CLK_RESET, "Clock Reset");
        configInput(INPUT_CLK, "Clock");
        configInput(INPUT_GLOBAL_TRIG, "Glboal Trigger");

        for (int i = 0; i < nCHANNELS; ++i)
        {
            auto s = std::to_string(i + 1);
            configInput(INPUT_CH_HOLD + i, "Hold " + s);
            configInput(INPUT_CH_TRIG + i, "Trigger " + s);
            configOutput(OUTPUT_CV + i, "Envelope Output " + s);
        }
    }

    // clock
    dsp::SchmittTrigger m_SchTrigClk;

    // global triggers
    dsp::SchmittTrigger m_SchTrigGlobalClkReset;
    dsp::SchmittTrigger m_SchTrigGlobalTrig;

    // channel triggers
    std::array<dsp::SchmittTrigger, nCHANNELS> m_SchTrigChTrig{};

    int m_CurrentChannel = 0;
    int m_GraphData[nCHANNELS][ENVELOPE_HANDLES] = {};
    int m_Modes[nCHANNELS] = {};
    int m_Ranges[nCHANNELS] = {};
    int m_TimeDivs[nCHANNELS] = {};
    bool m_bHold[nCHANNELS] = {};
    bool m_bGateMode[nCHANNELS] = {};
    int m_HoldPos[nCHANNELS] = {};
    bool m_bTrig[nCHANNELS] = {};

    int m_waveSet = 0;
    bool m_bCpy = false;

    std::atomic<bool> m_refreshWidgets{false};
    int m_BeatCount = 0;
    std::atomic<int> m_bTrigCountdown[nCHANNELS]{}, m_bHoldCountdown[nCHANNELS]{};

    std::string m_sTextLabel;

    //-----------------------------------------------------
    // Band_Knob
    //-----------------------------------------------------
    struct Band_Knob : Knob_Yellow2_26
    {
        SEQ_Envelope_8 *mymodule;
        int param;

        void onChange(const event::Change &e) override
        {
            ParamQuantity *paramQuantity = getParamQuantity();
            mymodule = (SEQ_Envelope_8 *)paramQuantity->module;

            if (mymodule)
                mymodule->m_EditData->m_fband = paramQuantity->getValue();

            RoundKnob::onChange(e);
        }
    };

    void ChangeChannel(int ch);

    // Overrides
    void JsonParams(bool bTo, json_t *root);
    void process(const ProcessArgs &args) override;
    json_t *dataToJson() override;
    void dataFromJson(json_t *rootJ) override;
    void onRandomize() override;
    void onReset() override;
};

//-----------------------------------------------------
// Seq_Triad2_Pause
//-----------------------------------------------------
void EnvelopeEditCALLBACK(void *pClass, float val)
{
    char strVal[10] = {};

    SEQ_Envelope_8 *mymodule;
    mymodule = (SEQ_Envelope_8 *)pClass;

    if (!pClass)
        return;

    snprintf(strVal, 10, "[%.3fV]", val);

    mymodule->m_sTextLabel = strVal;
}

//-----------------------------------------------------
// Procedure:   SynthEdit_WaveSmooth
//-----------------------------------------------------
void SynthEdit_WaveSmooth(void *pClass, int id, bool bOn)
{
    SEQ_Envelope_8 *m;
    m = (SEQ_Envelope_8 *)pClass;

    m->m_EditData->smoothWave(m->m_CurrentChannel, 0.25f);
    m->m_refreshWidgets = true;
}

//-----------------------------------------------------
// SEQ_Envelope_8_DrawMode
//-----------------------------------------------------
void SEQ_Envelope_8_DrawMode(void *pClass, int id, bool bOn)
{
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;
    mymodule->m_EditData->m_bDraw = bOn;
    mymodule->m_refreshWidgets = true;
}

//-----------------------------------------------------
// SEQ_Envelope_8_GateMode
//-----------------------------------------------------
void SEQ_Envelope_8_GateMode(void *pClass, int id, bool bOn)
{
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;
    mymodule->m_bGateMode[mymodule->m_CurrentChannel] = bOn;
    mymodule->m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   SEQ_Envelope_8_ChSelect
//-----------------------------------------------------
void SEQ_Envelope_8_ChSelect(void *pClass, int id, int nbutton, bool bOn)
{
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;

    mymodule->ChangeChannel(nbutton);
    mymodule->m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   SEQ_Envelope_8_ModeSelect
//-----------------------------------------------------
void SEQ_Envelope_8_ModeSelect(void *pClass, int id, int nbutton, bool bOn)
{
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;

    mymodule->m_Modes[mymodule->m_CurrentChannel] = nbutton;
    mymodule->m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   SEQ_Envelope_8_TimeSelect
//-----------------------------------------------------
void SEQ_Envelope_8_TimeSelect(void *pClass, int id, int nbutton, bool bOn)
{
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;

    mymodule->m_TimeDivs[mymodule->m_CurrentChannel] = nbutton;
    mymodule->m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   SEQ_Envelope_8_RangeSelect
//-----------------------------------------------------
void SEQ_Envelope_8_RangeSelect(void *pClass, int id, int nbutton, bool bOn)
{
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;

    mymodule->m_Ranges[mymodule->m_CurrentChannel] = nbutton;
    mymodule->m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   SEQ_Envelope_8_Hold
//-----------------------------------------------------
void SEQ_Envelope_8_Hold(void *pClass, int id, bool bOn)
{
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;

    mymodule->m_bHold[id] = bOn;
    mymodule->m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   SEQ_Envelope_8_WaveSet
//-----------------------------------------------------
void SEQ_Envelope_8_WaveSet(void *pClass, int id, bool bOn)
{
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;

    if (id == 0)
    {
        if (++mymodule->m_waveSet >= EnvelopeData::nPRESETS)
            mymodule->m_waveSet = 0;
    }
    else
    {
        if (--mymodule->m_waveSet < 0)
            mymodule->m_waveSet = EnvelopeData::nPRESETS - 1;
    }

    mymodule->m_EditData->m_EnvData[mymodule->m_CurrentChannel].Preset(mymodule->m_waveSet);
    mymodule->m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   SEQ_Envelope_8_WaveInvert
//-----------------------------------------------------
void SEQ_Envelope_8_WaveInvert(void *pClass, int id, bool bOn)
{
    int i;
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;

    for (i = 0; i < ENVELOPE_HANDLES; i++)
        mymodule->m_EditData->m_EnvData[mymodule->m_CurrentChannel].setVal(
            i, 1.0f - mymodule->m_EditData->m_EnvData[mymodule->m_CurrentChannel].m_HandleVal[i]);
    mymodule->m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   SEQ_Envelope_8_WaveRand
//-----------------------------------------------------
void SEQ_Envelope_8_WaveRand(void *pClass, int id, bool bOn)
{
    int i;
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;

    for (i = 0; i < ENVELOPE_HANDLES; i++)
        mymodule->m_EditData->m_EnvData[mymodule->m_CurrentChannel].setVal(i, random::uniform());
    mymodule->m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   SEQ_Envelope_8_WaveCopy
//-----------------------------------------------------
void SEQ_Envelope_8_WaveCopy(void *pClass, int id, bool bOn)
{
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;

    mymodule->m_bCpy = bOn;
    mymodule->m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   SEQ_Envelope_8_Trig
//-----------------------------------------------------
void SEQ_Envelope_8_Trig(void *pClass, int id, bool bOn)
{
    SEQ_Envelope_8 *mymodule;

    if (!pClass)
        return;

    mymodule = (SEQ_Envelope_8 *)pClass;

    // global reset
    if (id == nCHANNELS)
    {
        // turn on all trigs
        for (int i = 0; i < nCHANNELS; i++)
        {
            mymodule->m_bTrig[i] = true;
            mymodule->m_bTrigCountdown[i] = 5;
        }
    }
    else
    {
        mymodule->m_bTrig[id] = true;
        mymodule->m_bTrigCountdown[id] = 5;
    }

    mymodule->m_refreshWidgets = true;
}

std::weak_ptr<Widget_EnvelopeEdit::EditData> S8W_browserEditData;

//-----------------------------------------------------
// Procedure:   Widget
//
//-----------------------------------------------------

struct SEQ_Envelope_8_Widget : ModuleWidget
{
    Widget_EnvelopeEdit *m_pEnvelope = NULL;
    MyLEDButtonStrip *m_pButtonChSelect = NULL;
    MyLEDButtonStrip *m_pButtonModeSelect = NULL;
    MyLEDButtonStrip *m_pButtonRangeSelect = NULL;
    MyLEDButtonStrip *m_pButtonTimeSelect = NULL;
    MyLEDButton *m_pButtonHold[nCHANNELS] = {};
    MyLEDButton *m_pButtonTrig[nCHANNELS] = {};
    MyLEDButton *m_pButtonGateMode = NULL;
    MyLEDButton *m_pButtonWaveSetBck = NULL;
    MyLEDButton *m_pButtonWaveSetFwd = NULL;
    MyLEDButton *m_pButtonJoinEnds = NULL;
    MyLEDButton *m_pButtonGlobalReset = NULL;
    MyLEDButton *m_pButtonDraw = NULL;
    MyLEDButton *m_pButtonCopy = NULL;
    MyLEDButton *m_pButtonRand = NULL;
    MyLEDButton *m_pButtonInvert = NULL;
    MyLEDButton *m_pButtonSmooth = NULL;

    Label *m_pTextLabel = NULL;

    SEQ_Envelope_8_Widget(SEQ_Envelope_8 *module)
    {
        int ch, x = 0, y = 0;

        // box.size = Vec( 15*36, 380);

        setModule(module);

        // box.size = Vec( 15*21, 380);
        setPanel(APP->window->loadSvg(asset::plugin(thePlugin, "res/SEQ_Envelope_8.svg")));

        addChild(createWidget<ScrewSilver>(Vec(15, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
        addChild(createWidget<ScrewSilver>(Vec(15, 365)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

        // input clock
        addInput(createInput<MyPortInSmall>(Vec(45, 18), module, SEQ_Envelope_8::INPUT_CLK));

        // input clock reset
        addInput(createInput<MyPortInSmall>(Vec(21, 18), module, SEQ_Envelope_8::INPUT_CLK_RESET));

        // invert
        m_pButtonInvert =
            new MyLEDButton(95, 23, 11, 11, 8.0, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
                            MyLEDButton::TYPE_MOMENTARY, 0, module, SEQ_Envelope_8_WaveInvert);
        addChild(m_pButtonInvert);

        // smooth
        m_pButtonSmooth =
            new MyLEDButton(125, 23, 11, 11, 8.0, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
                            MyLEDButton::TYPE_MOMENTARY, 0, module, SynthEdit_WaveSmooth);
        addChild(m_pButtonSmooth);

        // envelope editor
        std::shared_ptr<Widget_EnvelopeEdit::EditData> ed;
        if (module)
        {
            ed = module->m_EditData;
        }
        else
        {
            if (auto ted = S8W_browserEditData.lock())
            {
                ed = ted;
            }
            else
            {
                ed = std::make_shared<Widget_EnvelopeEdit::EditData>();
                S8W_browserEditData = ed;
            }
        }
        m_pEnvelope = new Widget_EnvelopeEdit(47, 42, 416, 192, 7, ed, module, EnvelopeEditCALLBACK,
                                              nCHANNELS);
        addChild(m_pEnvelope);

        // envelope select buttons
        m_pButtonChSelect = new MyLEDButtonStrip(
            210, 23, 11, 11, 3, 8.0, nCHANNELS, false, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
            MyLEDButtonStrip::TYPE_EXCLUSIVE, 0, module, SEQ_Envelope_8_ChSelect);
        addChild(m_pButtonChSelect);

        m_pTextLabel = new Label();
        m_pTextLabel->box.pos = Vec(450, 10);
        m_pTextLabel->text = "----";
        addChild(m_pTextLabel);

        // wave set buttons
        x = 364;
        y = 23;
        m_pButtonWaveSetBck =
            new MyLEDButton(x, y, 11, 11, 8.0, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
                            MyLEDButton::TYPE_MOMENTARY, 0, module, SEQ_Envelope_8_WaveSet);
        addChild(m_pButtonWaveSetBck);

        m_pButtonWaveSetFwd =
            new MyLEDButton(x + 12, y, 11, 11, 8.0, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
                            MyLEDButton::TYPE_MOMENTARY, 1, module, SEQ_Envelope_8_WaveSet);
        addChild(m_pButtonWaveSetFwd);

        x = 405;

        // random
        m_pButtonRand =
            new MyLEDButton(x, y, 11, 11, 8.0, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
                            MyLEDButton::TYPE_MOMENTARY, 0, module, SEQ_Envelope_8_WaveRand);
        addChild(m_pButtonRand);

        x += 25;

        // copy
        m_pButtonCopy =
            new MyLEDButton(x, y, 11, 11, 8.0, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
                            MyLEDButton::TYPE_SWITCH, 0, module, SEQ_Envelope_8_WaveCopy);
        addChild(m_pButtonCopy);

        // mode select
        m_pButtonModeSelect = new MyLEDButtonStrip(109, 256, 11, 11, 8, 8.0, EnvelopeData::nMODES,
                                                   true, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
                                                   MyLEDButtonStrip::TYPE_EXCLUSIVE, 0, module,
                                                   SEQ_Envelope_8_ModeSelect);
        addChild(m_pButtonModeSelect);

        // time select
        m_pButtonTimeSelect = new MyLEDButtonStrip(
            272, 256, 11, 11, 8, 8.0, nTIMESETS, true, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
            MyLEDButtonStrip::TYPE_EXCLUSIVE, 0, module, SEQ_Envelope_8_TimeSelect);
        addChild(m_pButtonTimeSelect);

        // range select
        m_pButtonRangeSelect = new MyLEDButtonStrip(
            350, 256, 11, 11, 8, 8.0, EnvelopeData::nRANGES - 3, true, DWRGB(180, 180, 180),
            DWRGB(0, 255, 255), MyLEDButtonStrip::TYPE_EXCLUSIVE, 0, module,
            SEQ_Envelope_8_RangeSelect);
        addChild(m_pButtonRangeSelect);

        // draw mode
        m_pButtonDraw =
            new MyLEDButton(48, 237, 11, 11, 8.0, DWRGB(180, 180, 180), DWRGB(255, 128, 0),
                            MyLEDButton::TYPE_SWITCH, 0, module, SEQ_Envelope_8_DrawMode);
        addChild(m_pButtonDraw);

        // gate mode
        m_pButtonGateMode =
            new MyLEDButton(48, 254, 11, 11, 8.0, DWRGB(180, 180, 180), DWRGB(255, 128, 0),
                            MyLEDButton::TYPE_SWITCH, 0, module, SEQ_Envelope_8_GateMode);
        addChild(m_pButtonGateMode);

        // global reset input
        addInput(
            createInput<MyPortInSmall>(Vec(21, 313), module, SEQ_Envelope_8::INPUT_GLOBAL_TRIG));

        // global reseet button
        m_pButtonGlobalReset =
            new MyLEDButton(5, 315, 14, 14, 11.0, DWRGB(180, 180, 180), DWRGB(0, 255, 255),
                            MyLEDButton::TYPE_MOMENTARY, nCHANNELS, module, SEQ_Envelope_8_Trig);
        addChild(m_pButtonGlobalReset);

        // band knob
        addParam(createParam<SEQ_Envelope_8::Band_Knob>(Vec(59.5, 280), module,
                                                        SEQ_Envelope_8::PARAM_BAND));

        // inputs, outputs
        x = 21;
        y = 53;
        for (ch = 0; ch < nCHANNELS; ch++)
        {
            addInput(
                createInput<MyPortInSmall>(Vec(x, y), module, SEQ_Envelope_8::INPUT_CH_TRIG + ch));

            // trig button
            m_pButtonTrig[ch] = new MyLEDButton(x - 16, y + 2, 14, 14, 11.0, DWRGB(180, 180, 180),
                                                DWRGB(0, 255, 255), MyLEDButton::TYPE_MOMENTARY, ch,
                                                module, SEQ_Envelope_8_Trig);
            addChild(m_pButtonTrig[ch]);

            // hold button
            m_pButtonHold[ch] =
                new MyLEDButton(470, y + 2, 14, 14, 11.0, DWRGB(180, 180, 180), DWRGB(255, 0, 0),
                                MyLEDButton::TYPE_SWITCH, ch, module, SEQ_Envelope_8_Hold);
            addChild(m_pButtonHold[ch]);

            // hold gate input
            addInput(createInput<MyPortInSmall>(Vec(486, y), module,
                                                SEQ_Envelope_8::INPUT_CH_HOLD + ch));

            // out cv
            addOutput(
                createOutput<MyPortOutSmall>(Vec(516, y), module, SEQ_Envelope_8::OUTPUT_CV + ch));

            y += 28;
        }

        if (module)
        {
            module->m_refreshWidgets = true;
            module->m_bInitialized = true;
        }
    }

    void step() override
    {
        auto az = dynamic_cast<SEQ_Envelope_8 *>(module);
        if (az)
        {
            if (az->m_refreshWidgets)
            {
                az->m_refreshWidgets = false;
                for (auto ch = 0; ch < nCHANNELS; ch++)
                {

                    // hold button
                    m_pButtonHold[ch]->Set(az->m_bHold[ch]);

                    m_pEnvelope->setGateMode(ch, az->m_bGateMode[ch]);
                    m_pEnvelope->setMode(ch, az->m_Modes[ch]);
                    m_pEnvelope->setRange(ch, az->m_Ranges[ch]);
                    m_pEnvelope->setTimeDiv(ch, az->m_TimeDivs[ch]);
                    m_pEnvelope->setPos(ch, az->m_HoldPos[ch]);
                }

                m_pEnvelope->setView(az->m_CurrentChannel);
            }
            m_pTextLabel->text = az->m_sTextLabel;
            m_pButtonCopy->Set(az->m_bCpy);

            for (int ch = 0; ch < nCHANNELS; ++ch)
            {
                m_pButtonTrig[ch]->Set(az->m_bTrigCountdown[ch] > 0);
                m_pButtonHold[ch]->Set(az->m_bHoldCountdown[ch] > 0);
                if (az->m_bTrigCountdown[ch] > 0)
                    az->m_bTrigCountdown[ch]--;
                if (az->m_bHoldCountdown[ch] > 0)
                    az->m_bHoldCountdown[ch]--;
            }

            m_pEnvelope->recalcLine(-1, 0);
        }
        Widget::step();
    }
};

//-----------------------------------------------------
// Procedure: JsonParams
//
//-----------------------------------------------------
void SEQ_Envelope_8::JsonParams(bool bTo, json_t *root)
{
    JsonDataBool(bTo, "m_bHold", root, m_bHold, nCHANNELS);
    JsonDataBool(bTo, "m_bGateMode", root, m_bGateMode, nCHANNELS);
    JsonDataInt(bTo, "m_HoldPos", root, (int *)m_HoldPos, nCHANNELS);
    JsonDataInt(bTo, "m_TimeDivs", root, (int *)m_TimeDivs, nCHANNELS);
    JsonDataInt(bTo, "m_Modes", root, (int *)m_Modes, nCHANNELS);
    JsonDataInt(bTo, "m_Ranges", root, (int *)m_Ranges, nCHANNELS);
    JsonDataInt(bTo, "m_GraphData", root, (int *)m_GraphData, nCHANNELS * ENVELOPE_HANDLES);
}

//-----------------------------------------------------
// Procedure: toJson
//
//-----------------------------------------------------
json_t *SEQ_Envelope_8::dataToJson()
{
    json_t *root = json_object();

    if (!root)
        return NULL;

    m_EditData->getDataAll((int *)m_GraphData);

    for (int i = 0; i < nCHANNELS; i++)
        m_HoldPos[i] = m_EditData->getPos(i);

    JsonParams(TOJSON, root);

    return root;
}

//-----------------------------------------------------
// Procedure:   fromJson
//
//-----------------------------------------------------
void SEQ_Envelope_8::dataFromJson(json_t *root)
{
    JsonParams(FROMJSON, root);

    m_EditData->setDataAll((int *)m_GraphData);
    m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   onReset
//
//-----------------------------------------------------
void SEQ_Envelope_8::onReset()
{
    memset(m_GraphData, 0, sizeof(m_GraphData));
    memset(m_bGateMode, 0, sizeof(m_bGateMode));
    memset(m_Modes, 0, sizeof(m_Modes));
    memset(m_Ranges, 0, sizeof(m_Ranges));
    m_EditData->setDataAll((int *)m_GraphData);
    memset(m_bHold, 0, sizeof(m_bHold));

    ChangeChannel(0);
    m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   onRandomize
//
//-----------------------------------------------------
void SEQ_Envelope_8::onRandomize()
{
    int ch, i;

    for (ch = 0; ch < nCHANNELS; ch++)
    {
        for (i = 0; i < ENVELOPE_HANDLES; i++)
            m_EditData->m_EnvData[ch].setVal(i, random::uniform());
    }
    m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   onRandomize
//
//-----------------------------------------------------
void SEQ_Envelope_8::ChangeChannel(int ch)
{
    int i;

    if (ch < 0 || ch >= nCHANNELS)
        return;

    if (m_bCpy)
    {
        m_bCpy = false;

        for (i = 0; i < ENVELOPE_HANDLES; i++)
        {
            m_EditData->m_EnvData[ch].setVal(
                i, m_EditData->m_EnvData[m_CurrentChannel].m_HandleVal[i]);
        }

        m_TimeDivs[ch] = m_TimeDivs[m_CurrentChannel];
        m_Modes[ch] = m_Modes[m_CurrentChannel];
        m_Ranges[ch] = m_Ranges[m_CurrentChannel];
        m_bGateMode[ch] = m_bGateMode[m_CurrentChannel];
    }

    m_CurrentChannel = ch;
    m_refreshWidgets = true;
}

//-----------------------------------------------------
// Procedure:   step
//
//-----------------------------------------------------
void SEQ_Envelope_8::process(const ProcessArgs &args)
{
    int ch;
    bool bHold = false, bTrig = false, bGlobalTrig = false;
    // char strVal[ 10 ] = {};

    if (!m_bInitialized || !inputs[INPUT_CLK].isConnected())
        return;

    // global clock reset
    m_EditData->m_bClkReset =
        m_SchTrigGlobalClkReset.process(inputs[INPUT_CLK_RESET].getNormalVoltage(0.0f));

    m_BeatCount++;

    // track clock period
    if (m_SchTrigClk.process(inputs[INPUT_CLK].getNormalVoltage(0.0f)))
    {
        // sprintf( strVal, "%d", m_BeatCount );
        // m_pTextLabel->text = strVal;
        m_EditData->setBeatLen(m_BeatCount);
        m_BeatCount = 0;
    }

    if (m_SchTrigGlobalTrig.process(inputs[INPUT_GLOBAL_TRIG].getNormalVoltage(0.0f)))
    {
        bGlobalTrig = true;
    }

    // process each channel
    for (ch = 0; ch < nCHANNELS; ch++)
    {
        // trig, clock reset
        bTrig = (m_SchTrigChTrig[ch].process(inputs[INPUT_CH_TRIG + ch].getNormalVoltage(0.0f))) ||
                bGlobalTrig;

        if (bTrig)
            m_bTrigCountdown[ch] = 5;

        if (bTrig || m_bTrig[ch])
            bTrig = true;

        bHold = (m_bHold[ch] || inputs[INPUT_CH_HOLD + ch].getNormalVoltage(0.0f) > 2.5f);

        if (bHold)
            m_bHoldCountdown[ch] = 5;

        // process envelope
        outputs[OUTPUT_CV + ch].setVoltage(m_EditData->procStep(ch, bTrig, bHold));

        m_bTrig[ch] = false;
    }

    m_EditData->m_bClkReset = false;
}

Model *modelSEQ_Envelope_8 = createModel<SEQ_Envelope_8, SEQ_Envelope_8_Widget>("SEQ_Envelope_8");
