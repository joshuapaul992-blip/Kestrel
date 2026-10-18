/* ports/logind/kestrel-logind.c -- the part of systemd-logind a desktop's
 * logout dialog asks for, on the D-Bus system bus.
 *
 * LXDE's logout dialog (lxsession-logout) offers Shutdown and Reboot only
 * when org.freedesktop.login1 answers CanPowerOff and CanReboot with "yes",
 * then calls PowerOff or Reboot. Kestrel has no systemd, so this program
 * owns that name and answers those calls: power off and reboot are always
 * allowed (Kestrel runs the desktop as root), suspend and hibernate are not
 * available. PowerOff and Reboot reply first, flush the file systems and
 * then call reboot(2), which is what BusyBox's poweroff -f does.
 *
 * Built against GIO by tools/mkfirefoxroot.sh, installed as
 * /usr/libexec/kestrel-logind; /bin/startx starts it with the system bus. */
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>

static const char introspection[] =
    "<node>"
    "  <interface name='org.freedesktop.login1.Manager'>"
    "    <method name='CanPowerOff'><arg type='s' direction='out'/></method>"
    "    <method name='CanReboot'><arg type='s' direction='out'/></method>"
    "    <method name='CanHalt'><arg type='s' direction='out'/></method>"
    "    <method name='CanSuspend'><arg type='s' direction='out'/></method>"
    "    <method name='CanHibernate'><arg type='s' direction='out'/></method>"
    "    <method name='CanHybridSleep'><arg type='s' direction='out'/></method>"
    "    <method name='CanSuspendThenHibernate'><arg type='s' direction='out'/></method>"
    "    <method name='PowerOff'><arg type='b' direction='in'/></method>"
    "    <method name='Reboot'><arg type='b' direction='in'/></method>"
    "    <method name='Halt'><arg type='b' direction='in'/></method>"
    "  </interface>"
    "</node>";

static int pending_cmd;
static GMainLoop *loop;

static gboolean do_reboot(gpointer data)
{
    sync();
    reboot(pending_cmd);
    perror("kestrel-logind: reboot");
    return G_SOURCE_REMOVE;
}

static void method_call(GDBusConnection *conn, const gchar *sender, const gchar *path,
                        const gchar *iface, const gchar *method, GVariant *params,
                        GDBusMethodInvocation *inv, gpointer data)
{
    if (!strcmp(method, "CanPowerOff") || !strcmp(method, "CanReboot") || !strcmp(method, "CanHalt")) {
        g_dbus_method_invocation_return_value(inv, g_variant_new("(s)", "yes"));
    } else if (!strncmp(method, "Can", 3)) {
        g_dbus_method_invocation_return_value(inv, g_variant_new("(s)", "na"));
    } else {
        pending_cmd = !strcmp(method, "PowerOff") ? RB_POWER_OFF
                    : !strcmp(method, "Reboot") ? RB_AUTOBOOT : RB_HALT_SYSTEM;
        fprintf(stderr, "kestrel-logind: %s for %s\n", method, sender);
        g_dbus_method_invocation_return_value(inv, NULL);
        g_dbus_connection_flush_sync(conn, NULL, NULL);
        g_timeout_add(200, do_reboot, NULL);    /* let the caller see the reply */
    }
}

static const GDBusInterfaceVTable vtable = { .method_call = method_call };

static void bus_acquired(GDBusConnection *conn, const gchar *name, gpointer data)
{
    GDBusNodeInfo *node = data;
    GError *err = NULL;
    if (!g_dbus_connection_register_object(conn, "/org/freedesktop/login1", node->interfaces[0],
                                           &vtable, NULL, NULL, &err)) {
        fprintf(stderr, "kestrel-logind: %s\n", err->message);
        g_error_free(err);
    }
}

static void name_lost(GDBusConnection *conn, const gchar *name, gpointer data)
{
    fprintf(stderr, "kestrel-logind: could not own %s on the system bus\n", name);
    g_main_loop_quit(loop);
}

int main(void)
{
    GDBusNodeInfo *node = g_dbus_node_info_new_for_xml(introspection, NULL);
    loop = g_main_loop_new(NULL, FALSE);
    g_bus_own_name(G_BUS_TYPE_SYSTEM, "org.freedesktop.login1", G_BUS_NAME_OWNER_FLAGS_NONE,
                   bus_acquired, NULL, name_lost, node, NULL);
    g_main_loop_run(loop);
    return 1;
}
