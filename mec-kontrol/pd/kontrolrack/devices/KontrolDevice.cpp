#include "KontrolDevice.h"

#include "../../m_pd.h"

#include <unordered_set>
//#include <iostream>

KontrolDevice::KontrolDevice() {
    model_ = Kontrol::KontrolModel::model();
}

KontrolDevice::~KontrolDevice() {
    ;
}

void KontrolDevice::changeMode(unsigned mode) {
    currentMode_ = mode;
    auto m = modes_[mode];
    if (m != nullptr) m->activate();
}

void KontrolDevice::addMode(unsigned mode, std::shared_ptr<DeviceMode> handler) {
    modes_[mode] = handler;
}

bool KontrolDevice::init() {
    //FIXME: cannot create shared_ptr like this
    //model_->addCallback("pd.kdevice", std::shared_ptr<KontrolDevice>(this));
    midiLearn(Kontrol::CS_LOCAL, false);
    modulationLearn(Kontrol::CS_LOCAL, false);
    enableMenu(true);
    lastParamId_ = "";
    for (const auto &m : modes_) {
        if (m.second != nullptr) m.second->init();
    }
    return true;
}

void KontrolDevice::poll() {
    auto m = modes_[currentMode_];
    if (m != nullptr) m->poll();
}

void KontrolDevice::changePot(unsigned pot, float value) {
    auto m = modes_[currentMode_];
    if (m != nullptr) m->changePot(pot, value);
}

void KontrolDevice::changeEncoder(unsigned encoder, float value) {
    auto m = modes_[currentMode_];
    if (m != nullptr) m->changeEncoder(encoder, value);
}

void KontrolDevice::encoderButton(unsigned encoder, bool value) {
    auto m = modes_[currentMode_];
    if (m != nullptr) m->encoderButton(encoder, value);
}

void KontrolDevice::keyPress(unsigned key, unsigned value) {
    auto m = modes_[currentMode_];
    if (m != nullptr) m->keyPress(key, value);
}


void KontrolDevice::selectPage(unsigned page) {
    auto m = modes_[currentMode_];
    if (m != nullptr) m->selectPage(page);
}


void KontrolDevice::rack(Kontrol::ChangeSource src, const Kontrol::Rack &rack) {
    auto m = modes_[currentMode_];
    if (m != nullptr) m->rack(src, rack);
}

void KontrolDevice::module(Kontrol::ChangeSource src, const Kontrol::Rack &rack, const Kontrol::Module &module) {
    if (currentModuleId_.empty()) {
        currentRackId_ = rack.id();
        currentModule(module.id());
    }
    auto m = modes_[currentMode_];
    if (m != nullptr) m->module(src, rack, module);
}

void KontrolDevice::page(Kontrol::ChangeSource src, const Kontrol::Rack &rack, const Kontrol::Module &module,
                         const Kontrol::Page &page) {
    auto m = modes_[currentMode_];
    if (m != nullptr) m->page(src, rack, module, page);
}

void KontrolDevice::param(Kontrol::ChangeSource src, const Kontrol::Rack &rack, const Kontrol::Module &module,
                          const Kontrol::Parameter &param) {
    auto m = modes_[currentMode_];
    if (m != nullptr) m->param(src, rack, module, param);
}

void KontrolDevice::changed(Kontrol::ChangeSource src, const Kontrol::Rack &rack, const Kontrol::Module &module,
                            const Kontrol::Parameter &param) {
    // set last param for midi learn
    if ((midiLearnActive_ || modulationLearnActive_)
        && rack.id() == currentRackId_
        && module.id() == currentModuleId_) {
        lastParamId_ = param.id();
    }

    auto m = modes_[currentMode_];
    if (m != nullptr) m->changed(src, rack, module, param);
}

void KontrolDevice::resource(Kontrol::ChangeSource src, const Kontrol::Rack &rack, const std::string &resType,
                             const std::string &resValue) {
    auto m = modes_[currentMode_];
    if (m != nullptr) m->resource(src, rack, resType, resValue);

    if(resType=="moduleorder") {
        moduleOrder_.clear();
        if(resType.length()>0) {
            int lidx =0;
            int idx = 0;
            int len = 0;
            while((idx=resType.find(" ",lidx)) != std::string::npos) {
                len = idx - lidx;
                moduleOrder_.push_back(resType.substr(lidx,len));
                lidx = idx + 1;
            }
            len = resType.length() - lidx;
            if(len>0) moduleOrder_.push_back(resType.substr(lidx,len));
        }
    }
}

void KontrolDevice::deleteRack(Kontrol::ChangeSource, const Kontrol::Rack &) {
}

void KontrolDevice::loadModule(Kontrol::ChangeSource src, const Kontrol::Rack &rack,
                               const Kontrol::EntityId &moduleId, const std::string &modType) {
    auto m = modes_[currentMode_];
    if (m != nullptr) m->loadModule(src, rack, moduleId, modType);
}

void KontrolDevice::midiCC(unsigned num, unsigned value) {
    if (midiLearnActive_) {
        if (!lastParamId_.empty()) {
            auto rack = model()->getRack(currentRackId_);
            if (rack != nullptr) {
                if (value > 0) {
                    rack->addMidiCCMapping(num, currentModuleId_, lastParamId_);
                    lastParamId_ = "";
                } else {
                    //std::cerr << "midiCC unlearn" << num << " " << lastParamId_ << std::endl;
                    rack->removeMidiCCMapping(num, currentModuleId_, lastParamId_);
                    lastParamId_ = "";
                }
            }
        }
    }

    auto rack = model()->getLocalRack();
    if (rack != nullptr) {
        rack->changeMidiCC(num, value);
    }
}

void KontrolDevice:: modulate(const std::string& src, unsigned bus, float value) {
    //TODO: when adding src dependent modulation, check to see what we should use for mod learn
    if (modulationLearnActive_) {
        if (!lastParamId_.empty()) {
            auto rack = model()->getRack(currentRackId_);
            if (rack != nullptr) {
                if (value > 0.1) {
                    //std::cerr << "modulation learn" << bus << " " << lastParamId_ << std::endl;
                    rack->addModulationMapping("local", bus, currentModuleId_, lastParamId_);
                    lastParamId_ = "";
                } else {
                    //std::cerr << "modulation unlearn" << bus << " " << lastParamId_ << std::endl;
                    rack->removeModulationMapping("local", bus, currentModuleId_, lastParamId_);
                    lastParamId_ = "";
                }
            }
        }
    }

    auto rack = model()->getLocalRack();
    // update param model
    if (rack != nullptr) {
        rack->changeModulation(bus, value);
    }
}


void KontrolDevice::midiLearn(Kontrol::ChangeSource src, bool b) {
    lastParamId_ = "";
    modulationLearnActive_ = false;
    midiLearnActive_ = b;
}

void KontrolDevice::modulationLearn(Kontrol::ChangeSource src, bool b) {
    lastParamId_ = "";
    midiLearnActive_ = false;
    modulationLearnActive_ = b;
}

void KontrolDevice::midiLearn(bool b) {
    model()->midiLearn(Kontrol::CS_LOCAL, b);
}

void KontrolDevice::modulationLearn(bool b) {
    model()->modulationLearn(Kontrol::CS_LOCAL, b);
}

void KontrolDevice::activeModule(Kontrol::ChangeSource src, const Kontrol::Rack &rack, const Kontrol::Module &module) {
    currentModuleId_ = module.id();
    auto m = modes_[currentMode_];
    if (m != nullptr) m->activeModule(src, rack, module);
}

static t_pd *get_object(const char *s) {
    t_pd *x = gensym(s)->s_thing;
    return x;
}

void KontrolDevice::sendPdMessage(const char *obj, float f) {
    t_pd *sendobj = get_object(obj);
    if (!sendobj) {
        post("KontrolDevice::sendPdMessage to %s failed", obj);
        return;
    }

    t_atom a;
    SETFLOAT(&a, f);
    pd_forwardmess(sendobj, 1, &a);
}

void KontrolDevice::sendPdModuleMessage(const std::string& msg, const std::string& module,
        float v1) {
    std::string m = msg + "-" + module;
    t_pd *sendobj = get_object(m.c_str());
    if (!sendobj) {
//        post("KontrolDevice::sendPdModuleMessage to %s failed", m.c_str());
        return;
    }

    t_atom a[1];
    SETFLOAT(&a[0], v1);
    pd_forwardmess(sendobj, 1, a);
}

void KontrolDevice::sendPdModuleMessage(const std::string& msg, const std::string& module,
        const std::string& v1) {

    std::string m = msg + "-" + module;
    t_pd *sendobj = get_object(m.c_str());
    if (!sendobj) {
//        post("KontrolDevice::sendPdModuleMessage to %s failed", m.c_str());
        return;
    }

    t_atom a[1];
    SETSYMBOL(&a[0], gensym(v1.c_str()));
    pd_forwardmess(sendobj, 1, a);
}



void KontrolDevice::sendPdModuleMessage(const std::string& msg, const std::string& module,
        const std::string& v1, float v2) {

    std::string m = msg + "-" + module;
    t_pd *sendobj = get_object(m.c_str());
    if (!sendobj) {
//        post("KontrolDevice::sendPdModuleMessage to %s failed", m.c_str());
        return;
    }

    t_atom a[2];
    SETSYMBOL(&a[0], gensym(v1.c_str()));
    SETFLOAT(&a[1], v2);
    pd_forwardmess(sendobj, 2, a);
}



void KontrolDevice::currentModule(const Kontrol::EntityId &moduleId) {
    model()->activeModule(Kontrol::CS_LOCAL, currentRackId_, moduleId);
}


std::vector<std::shared_ptr<Kontrol::Module>> KontrolDevice::getModules(const std::shared_ptr<Kontrol::Rack>& pRack) {
    std::vector<std::shared_ptr<Kontrol::Module>> ret;
    auto modulelist = model_->getModules(pRack);
    std::unordered_set<std::string> done;

    for (auto mid : moduleOrder_) {
        auto pModule = model_->getModule(pRack, mid);
        if(pModule!=nullptr) {
            ret.push_back(pModule);
            done.insert(mid);
        }
    }
    for (auto pModule : modulelist) {
        if(done.find(pModule->id()) == done.end()) {
            ret.push_back(pModule);
        }
    }

    return ret;
}
