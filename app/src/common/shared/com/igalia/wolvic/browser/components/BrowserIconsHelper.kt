package com.igalia.wolvic.browser.components

import android.app.Activity
import android.content.Context
import android.graphics.Bitmap
import android.widget.ImageView
import com.igalia.wolvic.browser.engine.EngineProvider
import kotlinx.coroutines.Deferred
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine

// Small helper class to simplify getting favicons.
class BrowserIconsHelper(context: Context, engine: Engine, store: BrowserStore) {

    private val browserIcons: BrowserIcons;

    init {
        browserIcons =
            BrowserIcons(context.applicationContext, EngineProvider.createClient(context))
        browserIcons.install(engine, store)
    }

    fun loadIntoView(
        view: ImageView,
        url: String,
        size: IconRequest.Size = IconRequest.Size.DEFAULT
    ) {
        val request = IconRequest(url, size, emptyList(), null, false)
        browserIcons.loadIntoView(view, request, null, null)
    }

    fun loadIntoView(
        view: ImageView,
        url: String,
        size: IconRequest.Size = IconRequest.Size.DEFAULT,
        activity: Activity
    ) {
        val request = IconRequest(url, size, emptyList(), null, false)
        //browserIcons.loadIntoView(view, request, null, null)
        var deferredIcon = browserIcons.loadIcon(request);
        deferredIcon.invokeOnCompletion { cause ->
            if (cause == null) {
                activity.runOnUiThread{
                    var completeIcon = deferredIcon.getCompleted()
                    val targetBmp: Bitmap = completeIcon.bitmap.copy(Bitmap.Config.ARGB_8888, false)
                    view.setImageBitmap(targetBmp)
                }
            }
        }
    }


    fun loadIntoView(
        view: ImageView,
        url: String,
        resources: List<IconRequest.Resource> = emptyList(),
        size: IconRequest.Size = IconRequest.Size.DEFAULT
    ) {
        val request = IconRequest(url, size, resources, null, false)
        browserIcons.loadIntoView(view, request, null, null)
    }

    fun loadIntoView(
        view: ImageView,
        url: String,
        resources: List<IconRequest.Resource> = emptyList(),
        size: IconRequest.Size = IconRequest.Size.DEFAULT,
        activity: Activity
    ) {
        val request = IconRequest(url, size, resources, null, false)
        //browserIcons.loadIntoView(view, request, null, null)
        var deferredIcon = browserIcons.loadIcon(request);
        deferredIcon.invokeOnCompletion { cause ->
            if (cause == null) {
                activity.runOnUiThread{
                    var completeIcon = deferredIcon.getCompleted()
                    val targetBmp: Bitmap = completeIcon.bitmap.copy(Bitmap.Config.ARGB_8888, false)
                    view.setImageBitmap(targetBmp)
                }
            }
        }
    }
}
