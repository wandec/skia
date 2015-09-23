/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkCachingPixelRef.h"
#include "SkBitmapCache.h"
#include "SkRect.h"

bool SkCachingPixelRef::Install(SkImageGenerator* generator,
                                SkBitmap* dst) {
    SkASSERT(dst != nullptr);
    if (nullptr == generator) {
        return false;
    }
    const SkImageInfo info = generator->getInfo();
    if (!dst->setInfo(info)) {
        delete generator;
        return false;
    }
    SkAutoTUnref<SkCachingPixelRef> ref(new SkCachingPixelRef(info, generator, dst->rowBytes()));
    dst->setPixelRef(ref);
    return true;
}

SkCachingPixelRef::SkCachingPixelRef(const SkImageInfo& info,
                                     SkImageGenerator* generator,
                                     size_t rowBytes)
    : INHERITED(info)
    , fImageGenerator(generator)
    , fErrorInDecoding(false)
    , fRowBytes(rowBytes) {
    SkASSERT(fImageGenerator != nullptr);
}
SkCachingPixelRef::~SkCachingPixelRef() {
    delete fImageGenerator;
    // Assert always unlock before unref.
}

bool SkCachingPixelRef::onNewLockPixels(LockRec* rec) {
    if (fErrorInDecoding) {
        return false;  // don't try again.
    }

    const SkImageInfo& info = this->info();
    if (!SkBitmapCache::Find(
                this->getGenerationID(), info.bounds(), &fLockedBitmap)) {
        // Cache has been purged, must re-decode.
        if (!fLockedBitmap.tryAllocPixels(info, fRowBytes)) {
            fErrorInDecoding = true;
            return false;
        }
        if (!fImageGenerator->getPixels(info, fLockedBitmap.getPixels(), fRowBytes)) {
            fErrorInDecoding = true;
            return false;
        }
        fLockedBitmap.setImmutable();
        SkBitmapCache::Add(this, info.bounds(), fLockedBitmap);
    }

    // Now bitmap should contain a concrete PixelRef of the decoded image.
    void* pixels = fLockedBitmap.getPixels();
    SkASSERT(pixels != nullptr);
    rec->fPixels = pixels;
    rec->fColorTable = nullptr;
    rec->fRowBytes = fLockedBitmap.rowBytes();
    return true;
}

void SkCachingPixelRef::onUnlockPixels() {
    fLockedBitmap.reset();
}
