package com.igalia.wolvic.ui.views.home

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteException
import androidx.annotation.Keep
import androidx.annotation.WorkerThread
import com.igalia.wolvic.utils.AssetsReplicator
import java.io.BufferedOutputStream
import java.io.BufferedReader
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStreamReader
import kotlin.math.min


/**
 * @author : peitian.feng
 * time    : 2023/11/21 10:02
 * desc    : 首页
 */
@Keep
object HomePage {

    @WorkerThread
    @JvmStatic
    fun restoreShortcuts(context: Context) {
        // copy内置favicons至本地
        copyFavicons(context)
        // 检查本地是否有homepage.html文件
        val filePath = context.filesDir?.absolutePath + File.separator + "homepage/home_page.html"
        val file = File(filePath)
        if (file.exists()) {
            return
        } else {
            createOrExistsFile(file)
        }
        // 获取homepage.html内容
        var homepage: String = readAssetFile(context, "homepage/home_page.html")
        // 读取db中的shortcuts
        val mLocalStorageInterface = LocalStorageJavaScriptInterface(context)
        val def: String? = mLocalStorageInterface.getItem("shortcuts")
        if (!def.isNullOrEmpty()) {
            val history = "const HISTORY_SHORTCUTS = '${def}';"
            homepage = homepage.replace("const HISTORY_SHORTCUTS = null;", history)
            // 清理数据库
            mLocalStorageInterface.clear()
        }
        // homepage.html写入到本地文件
        try {
            BufferedOutputStream(FileOutputStream(file)).use { bos ->
                val bytes: ByteArray = homepage.toByteArray()
                val bufferSize = 1024 * 8
                var index = 0
                while (index < bytes.size) {
                    val count = min(bufferSize.toDouble(), (bytes.size - index).toDouble()).toInt()
                    bos.write(bytes, index, count)
                    index += count
                }
                bos.flush()
            }
        } catch (e: IOException) {
            e.printStackTrace()
        }
    }

    private fun copyFavicons(context: Context, callback: AssetsReplicator.Callback?= null) {
        val replicator = AssetsReplicator(context)
        replicator.setCallback(callback)
        val dstPath = context.filesDir.absolutePath
        replicator.start("homepage/favicon", dstPath)
    }

    private fun createOrExistsFile(file: File) {
        if (createOrExistsDir(file.parentFile)) {
            try {
                file.createNewFile()
            } catch (e: IOException) {
                e.printStackTrace()
            }
        }
    }

    private fun createOrExistsDir(file: File?): Boolean {
        return file != null && if (file.exists()) file.isDirectory else file.mkdirs()
    }

    private fun readAssetFile(context: Context, filepath: String): String {
        val inputStream = context.assets.open(filepath)
        val reader = BufferedReader(InputStreamReader(inputStream))
        val stringBuilder = StringBuilder()
        var line: String?
        try {
            while (reader.readLine().also { line = it } != null) {
                stringBuilder.append(line).append("\n")
            }
        } catch (e: IOException) {
            e.printStackTrace()
        } finally {
            try {
                inputStream.close()
            } catch (e: IOException) {
                e.printStackTrace()
            }
        }
        return stringBuilder.toString()
    }

    internal class LocalStorageJavaScriptInterface(private val context: Context) {

        private val localStorageDBHelper: LocalStorage by lazy {
            LocalStorage.getInstance(context)
        }

        /**
         * This method allows to get an item for the given key
         * @param key : the key to look for in the local storage
         * @return the item having the given key
         * @Throws SQLiteException – if the database cannot be opened
         */
        @Throws(SQLiteException::class)
        fun getItem(key: String): String? {
            var value: String? = null
            val database = localStorageDBHelper.readableDatabase
            val cursor = database.query(
                LocalStorage.LOCALSTORAGE_TABLE_NAME,
                null, LocalStorage.LOCALSTORAGE_ID + " = ?",
                arrayOf(key), null, null, null
            )
            if (cursor.moveToFirst()) {
                value = cursor.getString(1)
            }
            cursor.close()
            database.close()
            return value
        }

        /**
         * set the value for the given key, or create the set of data
         * if the key does not exist already.
         * @param key
         * @param value
         * @Throws SQLiteException – if the database cannot be opened for writing
         */
        @Throws(SQLiteException::class)
        fun setItem(key: String, value: String) {
            val oldValue = getItem(key)
            val database = localStorageDBHelper.writableDatabase
            val values = ContentValues()
            values.put(LocalStorage.LOCALSTORAGE_ID, key)
            values.put(LocalStorage.LOCALSTORAGE_VALUE, value)
            if (oldValue != null) {
                database.update(
                    LocalStorage.LOCALSTORAGE_TABLE_NAME,
                    values,
                    LocalStorage.LOCALSTORAGE_ID + "='" + key + "'",
                    null
                )
            } else {
                database.insert(LocalStorage.LOCALSTORAGE_TABLE_NAME, null, values)
            }
            database.close()
        }

        /**
         * removes the item corresponding to the given key
         * @param key
         * @Throws SQLiteException – if the database cannot be opened for writing
         */
        @Throws(SQLiteException::class)
        fun removeItem(key: String) {
            val database = localStorageDBHelper.writableDatabase
            database.delete(
                LocalStorage.LOCALSTORAGE_TABLE_NAME,
                LocalStorage.LOCALSTORAGE_ID + "='" + key + "'",
                null
            )
            database.close()
        }

        /**
         * clears all the local storage.
         * @Throws SQLiteException – if the database cannot be opened for writing
         */
        @Throws(SQLiteException::class)
        fun clear() {
            val database = localStorageDBHelper.writableDatabase
            database.delete(LocalStorage.LOCALSTORAGE_TABLE_NAME, null, null)
            database.close()
        }

    }

}