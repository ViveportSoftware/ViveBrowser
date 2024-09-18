package com.igalia.wolvic.utils

import android.annotation.SuppressLint
import android.content.Context
import androidx.arch.core.executor.ArchTaskExecutor
import java.io.*

/**
 * @author : peitian.feng
 * time    : 2023/6/12 14:35
 * desc    : 从assets目录文件夹拷贝到目标文件夹
 */
class AssetsReplicator(private var context: Context) : Runnable {

    private val fileSeparator = File.separator

    private var assetsName: String? = null

    private var dstDir: String? = null

    private var callback: Callback? = null

    @SuppressLint("RestrictedApi")
    fun start(dirName: String, dstDir: String?) {
        if (dstDir.isNullOrEmpty()) return
        if (dirName.endsWith(fileSeparator)) {
            assetsName = dirName.substring(0, dirName.length - 1)
        } else {
            assetsName = dirName
        }
        if (dstDir.endsWith(fileSeparator)) {
            this.dstDir = dstDir.substring(0, dstDir.length - 1)
        } else {
            this.dstDir = dstDir
        }
        ArchTaskExecutor.getIOThreadExecutor().execute(this)
    }

    fun setCallback(callback: Callback?): AssetsReplicator {
        this.callback = callback
        return this
    }

    override fun run() {
        if (assetsName == null || dstDir == null) {
            onFailure("path can not be null.")
            return
        }
        copyFromAssets(context, assetsName!!, dstDir!!)

        callback?.onSuccess(dstDir + fileSeparator + assetsName)
    }

    private fun copyFromAssets(context: Context, sourceDir: String, dstDir: String) {
        val assets = context.assets
        try {
            val filesNames = assets.list(sourceDir)
            if (filesNames == null) {
                onFailure("Assets dir is empty.")
                return
            }
            if (filesNames.isNotEmpty()) {
                var tempPath: String
                for (fileName in filesNames) {
                    // 补全assets资源路径
                    tempPath = sourceDir + fileSeparator + fileName
                    val childNames = assets.list(tempPath) ?: continue
                    if (childNames.isNotEmpty()) {
                        checkFolderExists(dstDir + File.separator + tempPath)
                        copyFromAssets(context, tempPath, dstDir)
                    } else {
                        // 单个文件
                        write2File(dstDir + fileSeparator + tempPath, assets.open(tempPath))
                    }
                }
            } else {
                // 单个文件
                write2File(dstDir + fileSeparator + sourceDir, assets.open(sourceDir))
            }
        } catch (e: IOException) {
            onFailure(e.message)
        }
    }

    private fun checkFolderExists(dirPath: String) {
        val file = File(dirPath)
        if (!file.exists() || !file.isDirectory) {
            file.mkdirs()
        }
    }

    @Throws(IOException::class)
    private fun write2File(filePath: String, _in: InputStream) {
        val file = File(filePath)
        if (file.exists()) {
            return
        } else {
            createOrExistsFile(file)
        }
        try {
            val os: OutputStream = FileOutputStream(file)
            val buffer = ByteArray(4096)
            var read: Int
            while (_in.read(buffer).also { read = it } != -1) {
                os.write(buffer, 0, read)
            }
            _in.close()
            os.flush()
            os.close()
        } catch (e: FileNotFoundException) {
            e.printStackTrace()
        }
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

    private fun onFailure(eMsg: String?) = callback?.onFailure(eMsg)

    interface Callback {
        /**
         * copy成功
         * @param output  copy后的资源路径
         */
        fun onSuccess(output: String)

        /**
         * copy失败
         * @param error   错误信息
         */
        fun onFailure(error: String?)
    }

}