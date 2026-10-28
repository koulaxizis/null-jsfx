/*
 * NULL JSFX - Downmix Check, framework-independent DSP core
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Port of DATA/Effects/null_jsfx/downmix_check.jsfx, computed in double in the EEL2 order: a
 * surround bed (12 pins, L R C LFE Ls Rs Lrs Rrs Ltf Rtf Ltr Rtr) played as it is, folded to
 * 5.1, to stereo Lo/Ro (ITU-R BS.775) or to mono, with one pin soloed if asked. Gains only.
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace nulldmx {

class DownmixCheckProcessor {
public:
    // @slider
    void setParams(double bed, double monitor, double centerDb, double surroundDb, double heightDb, double lfeDb,
                   double solo, double outDb)
    {
        fBed = int(bed);
        fMon = int(monitor);
        fC = std::pow(10.0, centerDb / 20);
        fS = std::pow(10.0, surroundDb / 20);
        fH = std::pow(10.0, heightDb / 20);
        fL = lfeDb > -60 ? std::pow(10.0, lfeDb / 20) : 0;
        fSolo = int(solo);
        fOut = std::pow(10.0, outDb / 20);
        fG71 = fBed >= 1 ? std::sqrt(0.5) : 1;
    }

    // @sample: in[12] in, out[12] out; inPeak/outPeak (12 each) keep the display's peaks
    void processSample(const double* s, double* o, double* inPeak, double* outPeak)
    {
        double i[12];
        for (int k = 0; k < 6; ++k)
            i[k] = s[k];
        i[6] = fBed >= 1 ? s[6] : 0; i[7] = fBed >= 1 ? s[7] : 0;
        for (int k = 8; k < 12; ++k)
            i[k] = fBed == 2 ? s[k] : 0;
        if (fSolo > 0)
            for (int k = 0; k < 12; ++k)
                if (fSolo != k + 1) i[k] = 0;
        for (int k = 0; k < 12; ++k)
            inPeak[k] = std::max(inPeak[k], std::fabs(i[k]));
        const double f0 = i[0] + i[8] * fH;
        const double f1 = i[1] + i[9] * fH;
        const double f4 = (i[4] + i[10] * fH + i[6]) * fG71;
        const double f5 = (i[5] + i[11] * fH + i[7]) * fG71;
        double lo = f0 + i[2] * fC + f4 * fS + i[3] * fL;
        double ro = f1 + i[2] * fC + f5 * fS + i[3] * fL;
        fPeak = std::max(fPeak, std::max(std::fabs(lo), std::fabs(ro)) * fOut);
        if (fMon == 0)
        {
            for (int k = 0; k < 12; ++k)
                o[k] = i[k] * fOut;
        }
        else if (fMon == 1)
        {
            o[0] = f0 * fOut; o[1] = f1 * fOut; o[2] = i[2] * fOut; o[3] = i[3] * fOut;
            o[4] = f4 * fOut; o[5] = f5 * fOut;
            for (int k = 6; k < 12; ++k) o[k] = 0;
        }
        else
        {
            if (fMon == 3)
                lo = ro = (lo + ro) * 0.5;
            o[0] = lo * fOut; o[1] = ro * fOut;
            for (int k = 2; k < 12; ++k) o[k] = 0;
        }
        for (int k = 0; k < 12; ++k)
            outPeak[k] = std::max(outPeak[k], std::fabs(o[k]));
    }

    // the fold's peak since the last call
    double takePeak() { const double p = fPeak; fPeak = 0; return p; }

private:
    int fBed = 2, fMon = 2, fSolo = 0;
    double fC = 1, fS = 1, fH = 1, fL = 0, fOut = 1, fG71 = 1, fPeak = 0;
};

} // namespace nulldmx
