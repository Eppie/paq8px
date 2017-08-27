#ifndef PAQ8PX_MODELS_H
#define PAQ8PX_MODELS_H

#include <cmath>
#include "../StateTable.h"
#include "../utils.h"
#include "../Ilog.h"
#include "../Stretch.h"
#include "../StateMap.h"
#include "../Mixer.h"
#include "../ContextMap.h"

inline U8 Clip(int Px) {
    return min(0xFF, max(0, Px));
}

inline U8 Clamp4(int Px, U8 n1, U8 n2, U8 n3, U8 n4) {
    return min(max(n1, max(n2, max(n3, n4))), max(min(n1, min(n2, min(n3, n4))), Px));
}

inline U8 LogMeanDiffQt(U8 a, U8 b) {
    return (a != b) ? ((a > b) << 3) | ilog2((a + b) / max(2, abs(a - b) * 2) + 1) : 0;
}

// Square buf(i)
inline int sqrbuf(int i) {
    assert(i > 0);
    return buf(i) * buf(i);
}

#include "matchModel.h"
#include "im1bitModel.h"
#include "im8bitModel.h"
#include "im24bitModel.h"
#include "recordModel.h"
#include "jpegmodel.h"
#include "matchModel.h"
#include "sparseModel.h"
#include "distanceModel.h"
#include "wavModel.h"
#include "wordModel.h"
#include "indirectModel.h"
#include "dmcModel.h"
#include "nestModel.h"
#include "exeModel.h"
#include "contextModel2.h"

#endif //PAQ8PX_MODELS_H
