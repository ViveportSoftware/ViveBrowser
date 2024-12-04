package com.igalia.wolvic.browser.api;

import android.os.Build;

public class WDefaultUserAgent {

    public static String getDefaultUserAgent(int mode) {
        String kWolvicUserAgent = getUserAgent() + " Mobile";
        String kWolvicUserAgentVR = getUserAgent() + " Mobile VR";
        String kLinuxInfoStr = "X11; Linux x86_64";
        String kWolvicUserAgentDesktop = buildUserAgentFromOSAndProduct(kLinuxInfoStr, getProductAndVersion());

        switch (mode) {
            case WSessionSettings.USER_AGENT_MODE_MOBILE:
                return kWolvicUserAgent;
            case WSessionSettings.USER_AGENT_MODE_DESKTOP:
                return kWolvicUserAgentDesktop;
            case WSessionSettings.USER_AGENT_MODE_VR:
            default:
                return kWolvicUserAgentVR;
        }
    }

    private static String buildUserAgentFromOSAndProduct(String kLinuxInfoStr, String productAndVersion) {
        String template = "Mozilla/5.0 (%s) AppleWebKit/537.36 (KHTML, like Gecko) %s Safari/537.36";
        return String.format(template, kLinuxInfoStr, productAndVersion);
    }

    private static String getProductAndVersion() {
        return "Chrome/121.0.0.0";
    }

    private static String getUserAgent() {
        String version = Build.VERSION.RELEASE;
        String name = Build.MODEL;
        String productAndVersion = getProductAndVersion();
        String template = "Mozilla/5.0 (Linux; Android %s; %s) AppleWebKit/537.36 (KHTML, like Gecko) %s Safari/537.36 OculusBrowser/17.2.0.9.61";
        return String.format(template, version, name, productAndVersion);
    }

}
