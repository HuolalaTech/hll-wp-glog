package glog.android.sample

import android.content.Context
import android.text.SpannableString
import android.text.Spanned
import android.text.style.ForegroundColorSpan
import android.util.AttributeSet
import android.view.View
import androidx.core.content.ContextCompat
import androidx.core.widget.NestedScrollView
import com.google.android.material.textview.MaterialTextView
import java.text.SimpleDateFormat
import java.util.*


/**
 * Created by issac on 2022/8/22.
 */
private enum class Level(val shorthand: String) {
    VERBOSE("V"),
    DEBUG("D"),
    INFO("I"),
    WARNING("W"),
    ERROR("E")
}

class LogCatView : MaterialTextView {
    private val verboseColor: Int by lazy { ContextCompat.getColor(context, R.color.log_verbose) }
    private val debugColor: Int by lazy { ContextCompat.getColor(context, R.color.log_debug) }
    private val infoColor: Int by lazy { ContextCompat.getColor(context, R.color.log_info) }
    private val warningColor: Int by lazy { ContextCompat.getColor(context, R.color.log_warning) }
    private val errorColor: Int by lazy { ContextCompat.getColor(context, R.color.log_error) }

    private val timeFormatter: SimpleDateFormat by lazy {
        SimpleDateFormat(
            "HH:mm:ss.SSS",
            Locale.US
        )
    }

    constructor(context: Context) : super(context) {
    }

    constructor(context: Context, attrs: AttributeSet?) : super(context, attrs) {
    }

    constructor(context: Context, attrs: AttributeSet?, defStyleAttr: Int) : super(
        context,
        attrs,
        defStyleAttr
    ) {
    }

    fun v(tag: String, message: String) {
        val logString = formatLog(Level.VERBOSE, tag, message)
        SpannableString(logString).apply {
            setSpan(
                ForegroundColorSpan(verboseColor),
                0,
                logString.length,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE
            )
        }.also { appendAndScrollToBottom(it) }
    }

    fun d(tag: String, message: String) {
        val logString = formatLog(Level.DEBUG, tag, message)
        SpannableString(logString).apply {
            setSpan(
                ForegroundColorSpan(debugColor),
                0,
                logString.length,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE
            )
        }.also { appendAndScrollToBottom(it) }
    }

    fun i(tag: String, message: String) {
        val logString = formatLog(Level.INFO, tag, message)
        SpannableString(logString).apply {
            setSpan(
                ForegroundColorSpan(infoColor),
                0,
                logString.length,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE
            )
        }.also { appendAndScrollToBottom(it) }
    }

    fun w(tag: String, message: String) {
        val logString = formatLog(Level.WARNING, tag, message)
        SpannableString(logString).apply {
            setSpan(
                ForegroundColorSpan(warningColor),
                0,
                logString.length,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE
            )
        }.also { appendAndScrollToBottom(it) }
    }

    fun e(tag: String, message: String) {
        val logString = formatLog(Level.ERROR, tag, message)
        SpannableString(logString).apply {
            setSpan(
                ForegroundColorSpan(errorColor),
                0,
                logString.length,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE
            )
        }.also { appendAndScrollToBottom(it) }
    }

    private fun appendAndScrollToBottom(text: CharSequence) {
        append(text)
        (parent?.parent as? NestedScrollView).run {
            post { this?.fullScroll(View.FOCUS_DOWN) }
        }
    }

    private fun formatLog(level: Level, tag: String, message: String): String {
        return "${if (text.isEmpty()) "" else "\n"}${timeFormatter.format(Date())} ${level.shorthand}/$tag: $message"
    }
}