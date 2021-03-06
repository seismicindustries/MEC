#include "mec_push2_param.h"

#include <mec_log.h>

#include <unordered_set>

namespace mec {
std::string centreText(const std::string t) {
    const unsigned max_len = 16;
    unsigned len = t.length();
    if (len > max_len) return t;
    const std::string pad = "                         ";
    unsigned padlen = (max_len - len) / 2;
    std::string ret = pad.substr(0, padlen) + t + pad.substr(0, padlen);
    // LOG_0("pad " << t << " l " << len << " pl " << padlen << "[" << ret << "]");
    return ret;
}


Push2API::Push2::Colour page_clrs[8] = {
        Push2API::Push2::Colour(0xFF, 0xFF, 0xFF),
        Push2API::Push2::Colour(0xFF, 0, 0xFF),
        Push2API::Push2::Colour(0xFF, 0, 0),
        Push2API::Push2::Colour(0, 0xFF, 0xFF),
        Push2API::Push2::Colour(0, 0xFF, 0),
        Push2API::Push2::Colour(0x7F, 0x7F, 0xFF),
        Push2API::Push2::Colour(0xFF, 0x7F, 0xFF),
        Push2API::Push2::Colour(0xFF, 0xFF, 0x7F)
};

P2_ParamMode::P2_ParamMode(mec::Push2 &parent, const std::shared_ptr<Push2API::Push2> &api)
        : parent_(parent),
          push2Api_(api),
          pageIdx_(-1),
          moduleIdx_(-1) {
    model_ = Kontrol::KontrolModel::model();
}

void P2_ParamMode::processNoteOn(unsigned, unsigned) {
    ;
}

void P2_ParamMode::processNoteOff(unsigned, unsigned) {
    ;
}


void P2_ParamMode::processCC(unsigned cc, unsigned v) {
    if (!v) return;

    if (cc >= P2_ENCODER_CC_START && cc <= P2_ENCODER_CC_END) {
        unsigned idx = cc - P2_ENCODER_CC_START;
        try {
            auto pRack = model_->getRack(parent_.currentRack());
            auto pModule = model_->getModule(pRack, parent_.currentModule());
            auto pPage = model_->getPage(pModule, parent_.currentPage());
            auto pParams = model_->getParams(pModule, pPage);

            if (idx >= pParams.size()) return;

            auto &param = pParams[idx];
            // auto page = pages_[currentPage_]
            if (param != nullptr) {
                const int steps = 1;
                // LOG_0("v = " << v);
                float value = v & 0x40 ? (128.0f - (float) v) / (128.0f * steps) * -1.0f : float(v) / (128.0f * steps);

                Kontrol::ParamValue calc = param->calcRelative(value);
                model_->changeParam(Kontrol::CS_LOCAL, parent_.currentRack(), pModule->id(), param->id(), calc);
            }
        } catch (std::out_of_range) {

        }

    } else if (cc >= P2_DEV_SELECT_CC_START && cc <= P2_DEV_SELECT_CC_END) {
        if (cc == P2_DEV_SELECT_CC_START && pageIdxOffset_ > 0) {
            pageIdxOffset_--;
            displayPage();
        } else if (cc == P2_DEV_SELECT_CC_END) {
            auto pRack = model_->getRack(parent_.currentRack());
            auto pModule = model_->getModule(pRack, parent_.currentModule());
            auto pPages = model_->getPages(pModule);
            if (pageIdxOffset_ + (8 - (pageIdxOffset_ > 0))
                < pPages.size()
                    ) {
                pageIdxOffset_++;
                displayPage();
            } else {
                unsigned idx = cc - P2_DEV_SELECT_CC_START + pageIdxOffset_ - (pageIdxOffset_ > 0);
                setCurrentPage(idx);
            }
        } else {
            unsigned idx = cc - P2_DEV_SELECT_CC_START + pageIdxOffset_ - (pageIdxOffset_ > 0);
            setCurrentPage(idx);
        }

    } else if (cc >= P2_TRACK_SELECT_CC_START && cc <= P2_TRACK_SELECT_CC_END) {
        unsigned idx = cc - P2_TRACK_SELECT_CC_START + moduleIdxOffset_;
        pageIdxOffset_ = 0;
        setCurrentModule(idx);
    } else if (cc == P2_SETUP_CC) {
        parent_.midiLearn(!parent_.midiLearn());
        parent_.sendCC(0, P2_SETUP_CC, (parent_.midiLearn() ? 0x7f : 0x10));
    } else if (cc == P2_AUTOMATE_CC) {
        parent_.modulationLearn(!parent_.modulationLearn());
        parent_.sendCC(0, P2_AUTOMATE_CC, (parent_.modulationLearn() ? 0x7f : 0x7b));
    } else if (cc == P2_CURSOR_LEFT_CC) {
        if (moduleIdxOffset_ > 0) {
            auto pRack = model_->getRack(parent_.currentRack());
            auto pModules = getModules(pRack);
            moduleIdxOffset_--;
            displayPage();
        }
    } else if (cc == P2_CURSOR_RIGHT_CC) {
        auto pRack = model_->getRack(parent_.currentRack());
        auto pModules = getModules(pRack);

        if (pModules.size() > (moduleIdxOffset_ + 8)) {
            moduleIdxOffset_++;
            displayPage();
        }
    } else if (cc == P2_USER_CC) {
        //DEBUG
        auto pRack = model_->getRack(parent_.currentRack());
        auto pModule = model_->getModule(pRack, parent_.currentModule());
        if (pModule != nullptr) pModule->dumpCurrentValues();
    }
}

void P2_ParamMode::drawParam(unsigned pos, const Kontrol::Parameter &param) {
    Push2API::Push2::Colour clr = page_clrs[pageIdx_ % 8];

    push2Api_->drawCell8(1, pos, centreText(param.displayName()).c_str(), clr);
    push2Api_->drawCell8(2, pos, centreText(param.displayValue()).c_str(), clr);
    push2Api_->drawCell8(3, pos, centreText(param.displayUnit()).c_str(), clr);
}

void P2_ParamMode::displayPage() {
//        int16_t clr = page_clrs[currentPage_];
    push2Api_->clearDisplay();
    for (unsigned int i = P2_DEV_SELECT_CC_START; i < P2_DEV_SELECT_CC_END; i++) { parent_.sendCC(0, i, 0); }
    for (unsigned int i = P2_TRACK_SELECT_CC_START; i < P2_TRACK_SELECT_CC_END; i++) { parent_.sendCC(0, i, 0); }

    auto pRack = model_->getRack(parent_.currentRack());
    auto pModules = getModules(pRack);
    auto pModule = model_->getModule(pRack, parent_.currentModule());
    auto pPage = model_->getPage(pModule, parent_.currentPage());
    auto pPages = model_->getPages(pModule);
    auto pParams = model_->getParams(pModule, pPage);

    // draw pages
    unsigned int i = 0;
    if (pageIdxOffset_ > 0) {
        push2Api_->drawCell8(0, i, centreText("<").c_str(), page_clrs[i]);
        i++;
    }

    auto piter = pPages.cbegin();
    for (i = 0; i < pageIdxOffset_ && piter != pPages.cend(); i++) {
        piter++;
    }


    for (i = (pageIdxOffset_ > 0); i < 8 && piter != pPages.cend(); i++, piter++) {
        int pidx = i - (pageIdxOffset_ > 0) + pageIdxOffset_;
        push2Api_->drawCell8(0, i, centreText((*piter)->displayName()).c_str(), page_clrs[pidx % 8]);
        parent_.sendCC(0, P2_DEV_SELECT_CC_START + i, pidx == pageIdx_ ? 122 : 124);
//        parent_.sendCC(0, P2_DEV_SELECT_CC_START + i, i + pageIdxOffset_ - ( pageIdxOffset_ > 0)   == pageIdx_ ? 122 : 124);

        auto pnext = piter;
        pnext++;
        if (i == 7 && pnext != pPages.cend()) {
            push2Api_->drawCell8(0, i, centreText(">").c_str(), page_clrs[i]);
        } else {
            if (pidx == pageIdx_) {
                unsigned int j = 0;
                for (auto param : pParams) {
                    if (param != nullptr) {
                        drawParam(j, *param);
                    }
                    j++;
                    if (j == 8) break;
                }
            }
        }
    }



    // draw modules
    parent_.sendCC(0, P2_CURSOR_LEFT_CC, moduleIdxOffset_ > 0 ? 0x75 : 0x00);
    i = 0;
    auto miter = pModules.cbegin();
    for (; i < moduleIdxOffset_ && miter != pModules.cend(); i++) {
        miter++;
    }

    for (i = 0; i < 8 && miter != pModules.cend(); miter++, i++) {
        std::string mname = (*miter)->id() + ":" + (*miter)->displayName();
        push2Api_->drawCell8(5, i, centreText(mname).c_str(), page_clrs[(i + moduleIdxOffset_) % 8]);
        parent_.sendCC(0, P2_TRACK_SELECT_CC_START + i, (i + moduleIdxOffset_) == moduleIdx_ ? 122 : 124);
    }

    parent_.sendCC(0, P2_CURSOR_RIGHT_CC, miter == pModules.cend() ? 0x00 : 0x7f);
}


void P2_ParamMode::setCurrentModule(int moduleIdx) {
    auto pRack = model_->getRack(parent_.currentRack());
    auto pModules = getModules(pRack);
//    auto pModule = model_->getModule(pRack, parent_.currentModule());
////    auto pPage = model_->getPage(pModule, parent_.currentPage());
//    auto pPages = model_->getPages(pModule);
////    auto pParams = model_->getParams(pModule, pPage);


    if (moduleIdx != moduleIdx_ && moduleIdx < pModules.size()) {
        moduleIdx_ = moduleIdx;

        try {
            parent_.currentModule(pModules[moduleIdx_]->id());

            pageIdx_ = -1;
            setCurrentPage(0);
        } catch (std::out_of_range) { ;
        }
    }
}


void P2_ParamMode::setCurrentPage(int pageIdx) {
    auto pRack = model_->getRack(parent_.currentRack());
    auto pModule = model_->getModule(pRack, parent_.currentModule());
//    auto pPage = model_->getPage(pModule, parent_.currentPage());
    auto pPages = model_->getPages(pModule);
//    auto pParams = model_->getParams(pModule, pPage);

    if (pPages.size() == 0) {
        parent_.currentPage("");
        displayPage();
    } else if (pageIdx != pageIdx_ && pageIdx < pPages.size()) {
        pageIdx_ = pageIdx;

        try {
            parent_.currentPage(pPages[pageIdx_]->id());
            displayPage();
        } catch (std::out_of_range) { ;
        }
    }
}

void P2_ParamMode::rack(Kontrol::ChangeSource src, const Kontrol::Rack &rack) {
    P2_DisplayMode::rack(src, rack);
    if (parent_.currentRack().length() == 0) parent_.currentRack(rack.id());
}

void P2_ParamMode::module(Kontrol::ChangeSource src, const Kontrol::Rack &rack, const Kontrol::Module &module) {
    P2_DisplayMode::module(src, rack, module);
    if (parent_.currentRack() != rack.id()) return;
    if (parent_.currentModule().length() == 0) {
        parent_.currentModule(module.id());
        setCurrentModule(0);
    } else {
        // adjust moduleidx
        auto pRack = model_->getRack(parent_.currentRack());
        auto pModules = getModules(pRack);
        unsigned int i = 0;
        for (auto mod : pModules) {
            if (mod->id() == parent_.currentModule()) {
                moduleIdx_ = i;

                if (module.type() != moduleType_) {
                    // type has changed, so need to update the display
                    pageIdx_ = -1;
                    parent_.currentPage("");
                }
                break;
            }
            i++;
        }

    }
    moduleType_ = module.type();
//    LOG_0("P2_ParamMode::module" << module.id());
    displayPage();
}

void P2_ParamMode::page(Kontrol::ChangeSource src, const Kontrol::Rack &rack, const Kontrol::Module &module,
                        const Kontrol::Page &page) {
    P2_DisplayMode::page(src, rack, module, page);
    if (parent_.currentRack() != rack.id()) return;
    if (parent_.currentModule() != module.id()) return;

    if (parent_.currentPage().length() == 0) {
        auto pRack = model_->getRack(parent_.currentRack());
        auto pModule = model_->getModule(pRack, parent_.currentModule());
        auto pPages = model_->getPages(pModule);
        if (pPages.size() > 0) {
            setCurrentPage(0);
        }
    }

    // LOG_0("P2_ParamMode::page " << page.id());
    displayPage();
}

void P2_ParamMode::param(Kontrol::ChangeSource src, const Kontrol::Rack &rack, const Kontrol::Module &module,
                         const Kontrol::Parameter &param) {
    P2_DisplayMode::param(src, rack, module, param);
//    LOG_0("P2_ParamMode::param " << param.id() << " : " << param.current().floatValue());

    if (parent_.currentRack() != rack.id()) return;
    if (parent_.currentModule() != module.id()) return;

    auto pRack = model_->getRack(parent_.currentRack());
    auto pModule = model_->getModule(pRack, parent_.currentModule());
    auto pPage = model_->getPage(pModule, parent_.currentPage());
//    auto pPages = model_->getPages(pModule);
    auto pParams = model_->getParams(pModule, pPage);

    unsigned i = 0;
    for (auto p: pParams) {
        if (p->id() == param.id()) {
            p->change(param.current(), src == Kontrol::CS_PRESET);
            drawParam(i, param);
            return;

        }
        i++;
    }
}

void P2_ParamMode::changed(Kontrol::ChangeSource src, const Kontrol::Rack &rack, const Kontrol::Module &module,
                           const Kontrol::Parameter &param) {
    P2_DisplayMode::changed(src, rack, module, param);
//    LOG_0("P2_ParamMode::changed " << param.id() << " : " << param.current().floatValue());
    auto pRack = model_->getRack(parent_.currentRack());
    auto pModule = model_->getModule(pRack, parent_.currentModule());
    auto pPage = model_->getPage(pModule, parent_.currentPage());
//    auto pPages = model_->getPages(pModule);
    auto pParams = model_->getParams(pModule, pPage);

    unsigned i = 0;
    for (auto p: pParams) {
        if (p->id() == param.id()) {
            p->change(param.current(), src == Kontrol::CS_PRESET);
            drawParam(i, param);
            return;
        }
        i++;
    }
}

std::vector<std::shared_ptr<Kontrol::Module>> P2_ParamMode::getModules(const std::shared_ptr<Kontrol::Rack>& pRack) {
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



void P2_ParamMode::resource(Kontrol::ChangeSource src,
        const Kontrol::Rack &rack,
        const std::string& resType, const std::string & res) {
    P2_DisplayMode::resource(src, rack, resType, resType);
    if(resType=="moduleorder") {
        moduleOrder_.clear();
        if(res.length()>0) {
            int lidx =0;
            int idx = 0;
            int len = 0;
            while((idx=res.find(" ",lidx)) != std::string::npos) {
                len = idx - lidx;
                moduleOrder_.push_back(res.substr(lidx,len));
                lidx = idx + 1;
            }
            len = res.length() - lidx;
            if(len>0) moduleOrder_.push_back(res.substr(lidx,len));
        }

        // redisplay page
        displayPage();
    }
}


void P2_ParamMode::activate() {
    P2_DisplayMode::activate();
    displayPage();
    for (int i = P2_DEV_SELECT_CC_START; i <= P2_DEV_SELECT_CC_END; i++) {
        parent_.sendCC(0, i, 0);
    }
    for (int i = P2_TRACK_SELECT_CC_START; i <= P2_TRACK_SELECT_CC_END; i++) {
        parent_.sendCC(0, i, 0);
    }
    parent_.sendCC(0, P2_CURSOR_LEFT_CC, 0x00);
    parent_.sendCC(0, P2_CURSOR_RIGHT_CC, 0x00);
    parent_.sendCC(0, P2_SETUP_CC, (parent_.midiLearn() ? 0x7f : 0x10));
    parent_.sendCC(0, P2_AUTOMATE_CC, (parent_.modulationLearn() ? 0x7f : 0x7b));
}


void P2_ParamMode::loadPreset(Kontrol::ChangeSource source, const Kontrol::Rack &rack, std::string preset) {
    P2_DisplayMode::loadPreset(source, rack, preset);
    setCurrentPage(0);
    displayPage();
}


void P2_ParamMode::midiLearn(Kontrol::ChangeSource src, bool b) {
    parent_.sendCC(0, P2_SETUP_CC, (parent_.midiLearn() ? 0x7f : 0x10));
}

void P2_ParamMode::modulationLearn(Kontrol::ChangeSource src, bool b) {
    parent_.sendCC(0, P2_AUTOMATE_CC, (parent_.modulationLearn() ? 0x7f : 0x7b));
}

void P2_ParamMode::activeModule(Kontrol::ChangeSource src, const Kontrol::Rack & rack, const Kontrol::Module &module) {
    if (parent_.currentRack() != rack.id()) return;
    if (parent_.currentModule() != module.id()) {

        auto pRack = model_->getRack(parent_.currentRack());
        auto pModules = getModules(pRack);
        int idx =0;
        for(auto mod : pModules) {
            if(mod->id() == module.id()) {
                setCurrentModule(idx);
                break;
            }
            idx++;
        }
    }
}

} //namespace
