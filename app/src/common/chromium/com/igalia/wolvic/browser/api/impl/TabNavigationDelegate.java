package com.igalia.wolvic.browser.api.impl;

import android.content.Context;
import com.igalia.wolvic.utils.InternalPages;

import org.chromium.wolvic.NavigationDelegate;

import java.lang.ref.WeakReference;

public class TabNavigationDelegate extends NavigationDelegate {

    private WeakReference<Context> mContext;

    public TabNavigationDelegate() {
        super();
    }

    @Override
    public String onLoadError(String uri, int error_code) {
        if (mContext == null) {
            return "";
        }

        Context context = mContext.get();
        if (context == null) {
            return "";
        }

        return InternalPages.createErrorPageDataURI(context,uri, error_code);
    }

    public void attach(Context context) {
        if (mContext != null) {
            mContext.clear();
            mContext = null;
        }
        mContext = new WeakReference<>(context);
    }

}