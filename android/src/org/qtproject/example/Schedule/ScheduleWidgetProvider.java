package org.qtproject.example.Schedule;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.util.Log;
import android.widget.RemoteViews;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * 桌面「今日课程」小组件。
 *
 * 数据来源：主程序把今天要上的课序列化成 JSON 写到 files/widget_today.json，
 * 再广播 WIDGET_REFRESH；小组件在这里读同一个文件并渲染。
 * 不在小组件里读 SQLite，避免重复实现课表/周次逻辑。
 */
public class ScheduleWidgetProvider extends AppWidgetProvider {
    public static final String JSON_FILE = "widget_today.json";
    public static final String ACTION_REFRESH = "org.qtproject.example.Schedule.WIDGET_REFRESH";

    @Override
    public void onUpdate(Context context, AppWidgetManager manager, int[] ids) {
        final String json = readJson(context);
        for (int id : ids) {
            render(context, manager, id, json);
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        super.onReceive(context, intent);
        if (ACTION_REFRESH.equals(intent.getAction())) {
            Log.i("ScheduleWidget", "refresh broadcast");
            final String json = readJson(context);
            final AppWidgetManager manager = AppWidgetManager.getInstance(context);
            final int[] ids = manager.getAppWidgetIds(
                    new ComponentName(context, ScheduleWidgetProvider.class));
            for (int id : ids) {
                render(context, manager, id, json);
            }
        }
    }

    private static String readJson(Context context) {
        final File file = new File(context.getFilesDir(), JSON_FILE);
        if (!file.exists()) {
            return "[]";
        }
        try (FileInputStream in = new FileInputStream(file)) {
            final byte[] data = new byte[(int) file.length()];
            final int read = in.read(data);
            return read > 0 ? new String(data, 0, read, "UTF-8") : "[]";
        } catch (Exception e) {
            Log.w("ScheduleWidget", "read widget json failed", e);
            return "[]";
        }
    }

    private static void render(Context context, AppWidgetManager manager, int id, String json) {
        Log.i("ScheduleWidget", "render id=" + id + " json=" + json);
        final RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.schedule_widget);
        views.setTextViewText(R.id.widget_date, dateLabel());

        final int[] slots = {
                R.id.widget_course_1, R.id.widget_course_2,
                R.id.widget_course_3, R.id.widget_course_4,
        };
        for (int slot : slots) {
            views.setTextViewText(slot, "");
        }

        int count = 0;
        try {
            final JSONArray array = new JSONArray(json);
            count = Math.min(array.length(), slots.length);
            for (int i = 0; i < count; ++i) {
                final JSONObject item = array.optJSONObject(i);
                views.setTextViewText(slots[i], item == null ? "" : item.optString("line", ""));
            }
        } catch (Exception ignored) {
            count = 0;
        }
        views.setTextViewText(R.id.widget_empty, count == 0 ? "今天没有课" : "");

        final Intent launch = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
        if (launch != null) {
            launch.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            final PendingIntent pending = PendingIntent.getActivity(context, 0, launch,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
            views.setOnClickPendingIntent(R.id.widget_root, pending);
        }
        manager.updateAppWidget(id, views);
    }

    private static String dateLabel() {
        final SimpleDateFormat format = new SimpleDateFormat("M 月 d 日 EEEE", Locale.CHINA);
        return format.format(new Date());
    }
}
