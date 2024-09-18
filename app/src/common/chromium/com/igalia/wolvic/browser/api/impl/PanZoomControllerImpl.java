package com.igalia.wolvic.browser.api.impl;

import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import com.igalia.wolvic.browser.api.WPanZoomController;

public class PanZoomControllerImpl implements WPanZoomController {
    SessionImpl mSession;

    public PanZoomControllerImpl(SessionImpl session) {
        mSession = session;
    }

    @Override
    public void onTouchEvent(@NonNull MotionEvent event) {
        ViewGroup contentView = getContentView();
        if (contentView != null) {
            contentView.dispatchTouchEvent(event);
        }
    }

    @Override
    public void onMotionEvent(@NonNull MotionEvent event) {
        ViewGroup contentView = getContentView();
        if (contentView != null) {
            contentView.dispatchGenericMotionEvent(event);
        }
    }

    private ViewGroup getContentView() {
        if (mSession == null) {
            return null;
        }
        return mSession.getContentView();
    }
}
