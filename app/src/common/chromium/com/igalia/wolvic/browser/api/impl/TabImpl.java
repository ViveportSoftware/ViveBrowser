package com.igalia.wolvic.browser.api.impl;

import android.content.Context;
import android.os.Looper;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.wolvic.Tab;

/**
 * Controlls a single tab content in a browser for chromium backend.
 */
public class TabImpl extends Tab {
    private TabMediaSessionObserver mTabMediaSessionObserver;
    private TabWebContentsDelegate mTabWebContentsDelegate;
    private TabWebContentsObserver mWebContentsObserver;
    private TabNavigationDelegate mTabNavigationDelegate;
    private final Looper mTabThread;


    public TabImpl(@NonNull Context context, @NonNull SessionImpl session, WebContents webContents) {
        super(context, session.getSettings().getUsePrivateMode(), webContents);
        mTabThread = Looper.myLooper();
        registerCallbacks(session);
        mTabNavigationDelegate.attach(context);
    }

    private void registerCallbacks(@NonNull SessionImpl session) {
        mTabMediaSessionObserver = new TabMediaSessionObserver(mWebContents, session);
        mTabWebContentsDelegate = new TabWebContentsDelegate(session, mWebContents);
        setWebContentsDelegate(mWebContents, mTabWebContentsDelegate);

        mWebContentsObserver = new TabWebContentsObserver(mWebContents, session);
        mTabNavigationDelegate = new TabNavigationDelegate();
        SelectionPopupController controller =
                SelectionPopupController.fromWebContents(mWebContents);
        controller.setDelegate(
                new SelectionPopupControllerDelegate(mWebContents,
                        controller.getDelegateEventHandler(), session));
    }

    public void exitFullScreen() {
        mWebContents.exitFullscreen();
    }

    public void onMediaResized(int width, int height) {
        mTabMediaSessionObserver.onMediaResized(width, height);
    }

    public void onMediaFullscreen(boolean isFullscreen) {
        mTabMediaSessionObserver.onMediaFullscreen(isFullscreen);
    }

    /**
     * Loads the given data into this Chromium Engine using a 'data' scheme URL.
     * @param data a String of data in the given encoding
     * @param mimeType the MIME type of the data, e.g. 'text/html'.
     * @param encoding the encoding of the data
     */
    public void loadData(@NonNull String data, @Nullable String mimeType,
                         @Nullable String encoding) {
        // Ignore mChromiumTabThread == null because this can be called during in the super class
        // constructor, before this class's own constructor has even started.
        if (mTabThread != null && Looper.myLooper() != mTabThread) {
            Throwable throwable = new Throwable(
                    "A ChromiumTab method was called on thread '" +
                            Thread.currentThread().getName() + "'. " +
                            "All ChromiumTab methods must be called on the same thread. " +
                            "(Expected Looper " + mTabThread + " called on " + Looper.myLooper() +
                            ", FYI main Looper is " + Looper.getMainLooper() + ")");
            throw new RuntimeException(throwable);
        }
        // Build LoadUrlParams
        String fixupData = TextUtils.isEmpty(data) ? "" : data;
        String fixupMimeType = TextUtils.isEmpty(mimeType) ? "text/html" : mimeType;
        boolean isBase64Encoded = "base64".equals(encoding);
        LoadUrlParams params = LoadUrlParams.createLoadDataParams(
                fixupData, fixupMimeType, isBase64Encoded);
        // Load
        mWebContents.getNavigationController().loadUrl(params);
    }

}
