package com.igalia.wolvic.browser.api.impl;

import android.util.Log;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.reflect.TypeToken;
import com.igalia.wolvic.browser.api.WSessionState;
import com.igalia.wolvic.ui.adapters.WebApp;
import com.igalia.wolvic.ui.widgets.Windows;

import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;

import java.lang.reflect.Type;
import java.util.ArrayList;

public class SessionStateImpl implements WSessionState {
    private NavigationHistory mNavigationHistory;

    public SessionStateImpl() {}

    public SessionStateImpl(NavigationHistory navigationHistory) {

        mNavigationHistory = navigationHistory;
    }

    @Override
    public boolean isEmpty() {
        return false;
    }

    @Override
    public String toJson() {
        Gson gson = new GsonBuilder().create();
        TypeToken<NavigationHistory> type = new TypeToken<NavigationHistory>(){};
        String json = gson.toJson(mNavigationHistory, type.getType());
        Log.d("VIVEBROWSER", "SessionStateImpl toJson: " + json);

        return json;
    }

    public static SessionStateImpl fromJson(String json) {

        Log.d("VIVEBROWSER", "SessionStateImpl fromJson: " + json);
        if(json == null || !json.contains("mCurrentEntryIndex")){
            Log.e("VIVEBROWSER", "SessionState json is not correct");
            return null;
        }

        Gson gson = new GsonBuilder().create();
        TypeToken<NavigationHistory> type = new TypeToken<NavigationHistory>(){};
        NavigationHistory navigationHistory = gson.fromJson(json, type);


        return new SessionStateImpl(navigationHistory);
    }

    public NavigationHistory getmNavigationHistory() {
        return mNavigationHistory;
    }
}