/* Included in the upstream HOG service only for the Z1PRO adaptation. */
#include <z1pro/low_latency_queue.h>
static struct z1_motion_queue z1_mouse_pending;
static struct k_spinlock z1_mouse_lock;
static uint32_t z1_notify_ok, z1_notify_errors;
void z1pro_perf_stats(uint32_t out[8]) {
    k_spinlock_key_t key=k_spin_lock(&z1_mouse_lock);
    uint32_t values[8]={z1_mouse_pending.produced,z1_mouse_pending.merged,
        z1_mouse_pending.overflow,z1_mouse_pending.count,z1_mouse_pending.max_depth,
        z1_notify_ok,z1_notify_errors,Z1_MOUSE_QUEUE_CAPACITY};
    memcpy(out,values,sizeof(values)); k_spin_unlock(&z1_mouse_lock,key);
}
void send_mouse_report_callback(struct k_work *work) {
    struct z1_motion_report pending;
    while (true) {
        k_spinlock_key_t key=k_spin_lock(&z1_mouse_lock);
        bool available=z1_motion_pop(&z1_mouse_pending,&pending);
        k_spin_unlock(&z1_mouse_lock,key);
        if (!available) return;
        struct bt_conn *conn=zmk_ble_active_profile_conn();
        if (!conn) {
            key=k_spin_lock(&z1_mouse_lock);z1_motion_clear(&z1_mouse_pending);
            k_spin_unlock(&z1_mouse_lock,key);return;
        }
        struct zmk_hid_mouse_report_body report={.buttons=pending.buttons,
            .d_x=pending.axis[0],.d_y=pending.axis[1],
            .d_scroll_y=pending.axis[2],.d_scroll_x=pending.axis[3]};
        struct bt_gatt_notify_params params={.attr=&hog_svc.attrs[13],
            .data=&report,.len=sizeof(report)};
        int err=bt_gatt_notify_cb(conn,&params);
        key=k_spin_lock(&z1_mouse_lock);
        if (err) z1_notify_errors++; else z1_notify_ok++;
        k_spin_unlock(&z1_mouse_lock,key);
        if (err==-EPERM) bt_conn_set_security(conn,BT_SECURITY_L2);
        bt_conn_unref(conn);
    }
}
K_WORK_DEFINE(hog_mouse_work,send_mouse_report_callback);
int zmk_hog_send_mouse_report(struct zmk_hid_mouse_report_body *report) {
    k_spinlock_key_t key=k_spin_lock(&z1_mouse_lock);
    z1_motion_push(&z1_mouse_pending,report->buttons,report->d_x,report->d_y,
                   report->d_scroll_y,report->d_scroll_x);
    k_spin_unlock(&z1_mouse_lock,key);
    k_work_submit_to_queue(&hog_work_q,&hog_mouse_work);return 0;
}
