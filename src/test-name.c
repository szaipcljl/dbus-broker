/*
 * Test Name Registry
 */

#include <c-macro.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "dbus/protocol.h"
#include "name.h"

static void test_setup(void) {
        NameRegistry registry;
        NameOwner owner, *o;
        NameChange change;
        int r;

        name_registry_init(&registry);
        name_owner_init(&owner);
        name_change_init(&change);

        r = name_registry_request_name(&registry, &owner, "foobar", 0, &change);
        assert(r == NAME_E_OWNER_NEW);
        assert(strcmp(change.name->name, "foobar") == 0);
        assert(change.old_owner == NULL);
        assert(change.new_owner == &owner);
        name_change_deinit(&change);
        o = name_registry_resolve_owner(&registry, "foobar");
        assert(o == &owner);
        r = name_registry_release_name(&registry, &owner, "foobar", &change);
        assert(r == 0);
        assert(strcmp(change.name->name, "foobar") == 0);
        assert(change.old_owner == &owner);
        assert(change.new_owner == NULL);
        name_change_deinit(&change);
        o = name_registry_resolve_owner(&registry, "foobar");
        assert(o == NULL);

        name_owner_deinit(&owner);
        name_registry_deinit(&registry);
}

static void test_release(void) {
        NameRegistry registry;
        NameOwner owner1, owner2;
        NameChange change;
        int r;

        name_registry_init(&registry);
        name_owner_init(&owner1);
        name_owner_init(&owner2);
        name_change_init(&change);

        r = name_registry_request_name(&registry, &owner1, "foobar", 0, &change);
        assert(r == NAME_E_OWNER_NEW);
        assert(strcmp(change.name->name, "foobar") == 0);
        assert(change.old_owner == NULL);
        assert(change.new_owner == &owner1);
        name_change_deinit(&change);

        r = name_registry_release_name(&registry, &owner1, "baz", &change);
        assert(r == NAME_E_NOT_FOUND);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);
        r = name_registry_release_name(&registry, &owner2, "foobar", &change);
        assert(r == NAME_E_NOT_OWNER);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);
        r = name_registry_release_name(&registry, &owner1, "foobar", &change);
        assert(r == 0);
        assert(strcmp(change.name->name, "foobar") == 0);
        assert(change.old_owner == &owner1);
        assert(change.new_owner == NULL);
        name_change_deinit(&change);

        name_owner_deinit(&owner2);
        name_owner_deinit(&owner1);
        name_registry_deinit(&registry);
}

static void test_queue(void) {
        NameRegistry registry;
        NameOwner owner1, owner2, *o;
        NameChange change;
        int r;

        name_registry_init(&registry);
        name_owner_init(&owner1);
        name_owner_init(&owner2);
        name_change_init(&change);

        /* first to request */
        r = name_registry_request_name(&registry, &owner1, "foobar", 0, &change);
        assert(r == NAME_E_OWNER_NEW);
        assert(strcmp(change.name->name, "foobar") == 0);
        assert(change.old_owner == NULL);
        assert(change.new_owner == &owner1);
        name_change_deinit(&change);
        /* verify the primary owner */
        o = name_registry_resolve_owner(&registry, "foobar");
        assert(o == &owner1);
        /* already the owner */
        r = name_registry_request_name(&registry, &owner1, "foobar", 0, &change);
        assert(r == 0);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);
        /* refuse to queue */
        r = name_registry_request_name(&registry, &owner2, "foobar", DBUS_NAME_FLAG_DO_NOT_QUEUE, &change);
        assert(r == NAME_E_EXISTS);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);
        /* try to overtake, but owner won't allow it */
        r = name_registry_request_name(&registry, &owner2, "foobar", DBUS_NAME_FLAG_DO_NOT_QUEUE | DBUS_NAME_FLAG_REPLACE_EXISTING, &change);
        assert(r == NAME_E_EXISTS);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);
        /* queue */
        r = name_registry_request_name(&registry, &owner2, "foobar", 0, &change);
        assert(r == NAME_E_IN_QUEUE_NEW);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);
        /* verify that the primary owner was untouched */
        o = name_registry_resolve_owner(&registry, "foobar");
        assert(o == &owner1);
        /* dequeu again */
        r = name_registry_release_name(&registry, &owner2, "foobar", &change);
        assert(r == 0);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);
        /* verify that the primary owner was untouched */
        o = name_registry_resolve_owner(&registry, "foobar");
        assert(o == &owner1);
        /* try to overtake, but wait in queue if it fails */
        r = name_registry_request_name(&registry, &owner2, "foobar", DBUS_NAME_FLAG_REPLACE_EXISTING, &change);
        assert(r == NAME_E_IN_QUEUE_NEW);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);
        /* again */
        r = name_registry_request_name(&registry, &owner2, "foobar", DBUS_NAME_FLAG_REPLACE_EXISTING, &change);
        assert(r == NAME_E_IN_QUEUE_UPDATED);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);
        /* update primary owner to allow replacement */
        r = name_registry_request_name(&registry, &owner1, "foobar", DBUS_NAME_FLAG_ALLOW_REPLACEMENT, &change);
        assert(r == 0);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);
        /* queue again, but do not attempt to overtake */
        r = name_registry_request_name(&registry, &owner2, "foobar", 0, &change);
        assert(r == NAME_E_IN_QUEUE_UPDATED);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);
        /* verify that the primary owner was untouched */
        o = name_registry_resolve_owner(&registry, "foobar");
        assert(o == &owner1);
        /* overtake primary owner, allow to be replaced ourselves and refuse to
         * queue */
        r = name_registry_request_name(&registry, &owner2, "foobar",
                                       DBUS_NAME_FLAG_REPLACE_EXISTING |
                                       DBUS_NAME_FLAG_ALLOW_REPLACEMENT |
                                       DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                       &change);
        assert(r == NAME_E_OWNER_UPDATED);
        assert(strcmp(change.name->name, "foobar") == 0);
        assert(change.old_owner == &owner1);
        assert(change.new_owner == &owner2);
        name_change_deinit(&change);
        /* verify that the primary owner was changed */
        o = name_registry_resolve_owner(&registry, "foobar");
        assert(o == &owner2);
        /* overtake again */
        r = name_registry_request_name(&registry, &owner1, "foobar", DBUS_NAME_FLAG_REPLACE_EXISTING, &change);
        assert(r == NAME_E_OWNER_UPDATED);
        assert(strcmp(change.name->name, "foobar") == 0);
        assert(change.old_owner == &owner2);
        assert(change.new_owner == &owner1);
        name_change_deinit(&change);
        /* verify that the primary owner was reverted to the original */
        o = name_registry_resolve_owner(&registry, "foobar");
        assert(o == &owner1);
        /* verify that the old primary owner is no longer on queue */
        r = name_registry_release_name(&registry, &owner2, "foobar", &change);
        assert(r = NAME_E_NOT_OWNER);
        assert(change.name == NULL);
        assert(change.old_owner == NULL);
        assert(change.new_owner == NULL);

        r = name_registry_release_name(&registry, &owner1, "foobar", &change);
        assert(r == 0);
        assert(strcmp(change.name->name, "foobar") == 0);
        assert(change.old_owner == &owner1);
        assert(change.new_owner == NULL);
        name_change_deinit(&change);

        name_owner_deinit(&owner2);
        name_owner_deinit(&owner1);
        name_registry_deinit(&registry);
}

int main(int argc, char **argv) {
        test_setup();
        test_release();
        test_queue();
        return 0;
}
