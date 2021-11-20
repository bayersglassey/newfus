

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "str_utils.h"
#include "lexer_macros.h"


/* Compiler "frames" are pushed onto a conceptual stack as we parse types
nested within other types (e.g. struct/union fields, array subtypes) */
typedef struct compiler_frame {
    const char *type_name;
    int array_depth;
} compiler_frame_t;


static int compiler_parse_type(compiler_t *compiler,
    compiler_frame_t *frame, type_t *type);
static int compiler_parse_type_ref(compiler_t *compiler,
    compiler_frame_t *frame, type_ref_t *ref);
static int compiler_parse_type_field(compiler_t *compiler,
    compiler_frame_t *frame, arrayof_inplace_type_field_t *fields);



void compiler_init(compiler_t *compiler, lexer_t *lexer,
    stringstore_t *store
) {
    memset(compiler, 0, sizeof(*compiler));
    compiler->lexer = lexer;
    compiler->store = store;
}

void compiler_cleanup(compiler_t *compiler) {
    ARRAY_FREE_PTR(compiler->defs, type_def_cleanup)
    ARRAY_FREE_PTR(compiler->bindings, compiler_binding_cleanup)
}

void compiler_dump(compiler_t *compiler, FILE *file) {
    fprintf(file, "Compiler %p:\n", compiler);

    fprintf(file, "Bindings:\n");
    for (int i = 0; i < compiler->bindings.len; i++) {
        compiler_binding_t *binding = compiler->bindings.elems[i];
        fprintf(file, "  %i/%zu: %s -> %s\n", i, compiler->bindings.len,
            binding->name, binding->def->name);
    }

    fprintf(file, "Defs:\n");
    for (int i = 0; i < compiler->defs.len; i++) {
        type_def_t *def = compiler->defs.elems[i];
        fprintf(file, "  %i/%zu: %s (%s)\n", i, compiler->defs.len,
            def->name, type_tag_string(def->type.tag));
        switch (def->type.tag) {
            case TYPE_TAG_ARRAY: {
                break;
            }
            case TYPE_TAG_STRUCT: case TYPE_TAG_UNION: {
                arrayof_inplace_type_field_t *fields = &def->type.u.struct_f.fields;
                for (int i = 0; i < fields->len; i++) {
                    type_field_t *field = &fields->elems[i];
                    fprintf(stderr, "    %s: %s\n", field->name,
                        type_tag_string(field->ref.type.tag));
                }
                break;
            }
            case TYPE_TAG_ALIAS: {
                fprintf(stderr, "    -> %s\n", def->type.u.alias_f.def->name);
                break;
            }
            default: break;
        }
    }
}

void compiler_binding_cleanup(compiler_binding_t *binding) {
    /* Nuthin */
}



static const char *compiler_get_packaged_name(compiler_t *compiler, const char *name) {
    if (!compiler->package_name) return name;
    return _const_strjoin3(compiler->store,
        compiler->package_name, "_", name);
}

static compiler_binding_t *compiler_get_binding(compiler_t *compiler,
    const char *name
) {
    for (int i = 0; i < compiler->bindings.len; i++) {
        compiler_binding_t *binding = compiler->bindings.elems[i];
        if (_streq(binding->name, name)) return binding;
    }
    return NULL;
}

static type_def_t *compiler_get_def(compiler_t *compiler,
    const char *type_name
) {
    for (int i = 0; i < compiler->defs.len; i++) {
        type_def_t *def = compiler->defs.elems[i];
        if (_streq(def->name, type_name)) return def;
    }
    return NULL;
}

static int compiler_add_def(compiler_t *compiler,
    const char *type_name, type_def_t **def_ptr
) {
    /* NOTE: caller guarantees no def exists with name type_name */
    ARRAY_PUSH_NEW(type_def_t*, compiler->defs, def)
    def->name = type_name;
    def->type.tag = TYPE_TAG_UNDEFINED;
    *def_ptr = def;
    return 0;
}

static int compiler_get_or_add_def(compiler_t *compiler,
    const char *type_name, type_def_t **def_ptr
) {
    int err;

    type_def_t *def = compiler_get_def(compiler, type_name);
    if (!def) {
        err = compiler_add_def(compiler, type_name, &def);
        if (err) return err;
    }

    *def_ptr = def;
    return 0;
}

static int compiler_redef_or_add_def(compiler_t *compiler,
    const char *type_name, type_def_t **def_ptr
) {
    int err;

    type_def_t *def = compiler_get_def(compiler, type_name);
    if (def) {
        if (def->type.tag == TYPE_TAG_UNDEFINED) {
            /* All good */
        } else if (compiler->can_redef) {
            /* Undefine it */
            type_def_cleanup(def);
            def->type.tag = TYPE_TAG_UNDEFINED;
        } else {
            fprintf(stderr, "Can't redefine: %s\n", type_name);
            return 2;
        }
    } else {
        err = compiler_add_def(compiler, type_name, &def);
        if (err) return err;
    }

    *def_ptr = def;
    return 0;
}

static int compiler_parse_type_ref(compiler_t *compiler,
    compiler_frame_t *frame, type_ref_t *ref
) {
    int err;
    lexer_t *lexer = compiler->lexer;

    memset(ref, 0, sizeof(*ref));
    ref->type.tag = TYPE_TAG_UNDEFINED;

    for (;;) {
        if (GOT("inplace")) {
            NEXT
            ref->inplace = true;
        } else if (GOT("weakref")) {
            NEXT
            ref->weakref = true;
        } else {
            break;
        }
    }

    err = compiler_parse_type(compiler, frame, &ref->type);
    if (err) return err;

    return 0;
}

static int compiler_parse_type_field(compiler_t *compiler,
    compiler_frame_t *frame, arrayof_inplace_type_field_t *fields
) {
    int err;
    lexer_t *lexer = compiler->lexer;

    const char *field_name;
    GET_CONST_NAME(field_name, compiler->store)

    ARRAY_PUSH(type_field_t, *fields, field)
    field->name = field_name;
    memset(&field->ref, 0, sizeof(type_ref_t));
    field->ref.type.tag = TYPE_TAG_UNDEFINED;

    const char *field_type_name = _const_strjoin3(compiler->store, frame->type_name,
        "_", field_name);
    if (!field_type_name) return 1;

    GET_OPEN
    compiler_frame_t subframe = *frame;
    /* Reset array_depth -- we are now a fresh "top-level" type, not some
    kind of array */
    subframe.array_depth = 0;
    subframe.type_name = field_type_name;
    err = compiler_parse_type_ref(compiler, &subframe, &field->ref);
    if (err) return err;
    GET_CLOSE

    return 0;
}

static int compiler_parse_type(compiler_t *compiler,
    compiler_frame_t *frame, type_t *type
) {
    int err;
    lexer_t *lexer = compiler->lexer;

    /* TODO: Is this where we look for "inplace" and "weakref"?
    Hmmm... does it make sense to have an array of inplace things? Yes.
    Array of weakrefs? Yes.
    Typedef defining a weakref to some other type? Not really.
    -> Being a weakref is not something which translates into C.
        That's why type_ref_t exists: it tells you how the host type
        (struct/union/array) is going to use the subtype.
    Typedef defining an inplace version of some other type? NO.
    -> Because defining a struct is, in C, already defining its "inplace"
        version.
        So what are we trying to achieve here?..
        I think basically, we want to hide the details of pointers away.
        So again, that's why we distinguish type_ref_t from type_t.
    */

    if (GOT("@")) {
        NEXT

        const char *name;
        GET_CONST_NAME(name, compiler->store)

        /* Get def */
        type_def_t *def;
        compiler_binding_t *binding = compiler_get_binding(compiler, name);
        if (binding) {
            /* Name refers to an existing binding: use its def */
            def = binding->def;
        } else {
            /* Name is expanded to a fully-qualified type name: get its def,
            or create a new undefined def */

            const char *packaged_name = compiler_get_packaged_name(compiler,
                name);
            if (!packaged_name) return 1;

            err = compiler_get_or_add_def(compiler, packaged_name, &def);
            if (err) return err;
        }

        /* Type is an alias to def */
        type->tag = TYPE_TAG_ALIAS;
        type->u.alias_f.def = def;
    } else if (GOT("@@")) {
        NEXT

        const char *packaged_name;
        GET_CONST_NAME(packaged_name, compiler->store)

        /* Get def */
        type_def_t *def;
        err = compiler_get_or_add_def(compiler, packaged_name, &def);
        if (err) return err;

        /* Type is an alias to def */
        type->tag = TYPE_TAG_ALIAS;
        type->u.alias_f.def = def;
    } else if (GOT("void")) {
        NEXT
        type->tag = TYPE_TAG_VOID;
    } else if (GOT("any")) {
        NEXT
        type->tag = TYPE_TAG_ANY;
    } else if (GOT("int")) {
        NEXT
        type->tag = TYPE_TAG_INT;
    } else if (GOT("sym")) {
        NEXT
        type->tag = TYPE_TAG_SYM;
    } else if (GOT("bool")) {
        NEXT
        type->tag = TYPE_TAG_BOOL;
    } else if (GOT("byte")) {
        NEXT
        type->tag = TYPE_TAG_BYTE;
    } else if (GOT("array")) {
        NEXT

        type_ref_t *ref = calloc(1, sizeof(*ref));
        if (!ref) return 1;
        ref->type.tag = TYPE_TAG_UNDEFINED;

        type->tag = TYPE_TAG_ARRAY;
        type->u.array_f.subtype_ref = ref;

        const char *elem_type_name = frame->type_name;
        if (!frame->array_depth) {
            const char *_elem_type_name = _const_strjoin2(compiler->store,
                elem_type_name, "_elem");
            if (!elem_type_name) return 1;

            elem_type_name = _elem_type_name;
        }

        GET_OPEN
        compiler_frame_t subframe = *frame;
        subframe.array_depth++;
        subframe.type_name = elem_type_name;
        err = compiler_parse_type_ref(compiler, &subframe, ref);
        if (err) return err;
        GET_CLOSE

        /* !!! TODO !!!
        This is not sufficient. We need to look at whether our subtype ref is
        inline/weakref, and add prefixes for those accordingly. */
        const char *array_type_name = _const_strjoin2(compiler->store,
            "arrayof_", elem_type_name);
        if (!array_type_name) return 1;

        type_def_t *def;
        err = compiler_get_or_add_def(compiler, array_type_name, &def);
        if (err) return err;
        /* TODO: get or create the array type for our subtype.
        E.g. "arrayof_int", "arrayof_thing",
        "arrayof_inplace_thing_field_elem"...
        How is this even possible?
        What do we do when e.g. subtype is undefined?
        When compilation is finished, no types should be left undefined.
        But until then... we won't be able to e.g. reliably check whether subtype
        is a struct/union and get its name.

        ...also, I think (like for "struct" and "union", see below) that the
        array type should live on its def, and the type we return to caller
        here should just be an alias to it. */
    } else if (GOT("struct") || GOT("union")) {
        bool is_union = lexer->token[0] == 'u';
        NEXT

        /* !!! TODO !!!
        The following will explode if we are doing e.g. "typedef lala: struct: ..."
        because both the "typedef" and the "struct" will be trying to define a type
        named "lala".
        So do we perhaps want to add a separate array of structs to the compiler,
        alongside the array of defs?.. */
        type_def_t *def;
        err = compiler_redef_or_add_def(compiler, frame->type_name, &def);
        if (err) return err;

        def->type.tag = is_union? TYPE_TAG_UNION: TYPE_TAG_STRUCT;
        def->type.u.struct_f.def = def;
        ARRAY_ZERO(def->type.u.struct_f.fields)

        GET_OPEN
        while (!DONE && !GOT_CLOSE) {
            err = compiler_parse_type_field(compiler, frame,
                &def->type.u.struct_f.fields);
            if (err) return err;
        }
        GET_CLOSE

        /* Caller gets an *alias* to our struct/union type */
        type->tag = TYPE_TAG_ALIAS;
        type->u.alias_f.def = def;
    } else {
        return UNEXPECTED(
            "one of: void any int sym bool byte array struct union");
    }

    return 0;
}

static int _compiler_compile(compiler_t *compiler) {
    int err;
    lexer_t *lexer = compiler->lexer;

    while (!DONE && !GOT_CLOSE) {
        if (GOT("typedef")) {
            NEXT

            const char *name;
            GET_CONST_NAME(name, compiler->store)

            const char *packaged_name = compiler_get_packaged_name(compiler,
                name);
            if (!packaged_name) return 1;

            type_def_t *def;
            err = compiler_redef_or_add_def(compiler, packaged_name, &def);
            if (err) return err;

            GET_OPEN
            compiler_frame_t frame = {
                .type_name = packaged_name,
            };
            err = compiler_parse_type(compiler, &frame, &def->type);
            if (err) return err;
            GET_CLOSE
        } else if (GOT("package")) {
            NEXT

            const char *package_name;
            GET_OPEN
            GET_CONST_NAME(package_name, compiler->store)
            GET_CLOSE

            compiler->package_name = package_name;
        } else if (GOT("from")) {
            NEXT

            const char *from_package_name;
            GET_CONST_NAME(from_package_name, compiler->store)

            GET_OPEN
            while (!DONE && !GOT_CLOSE) {
                const char *name;
                GET_CONST_NAME(name, compiler->store)

                const char *packaged_name = _const_strjoin3(compiler->store,
                    from_package_name, "_", name);
                if (!packaged_name) return 1;

                type_def_t *def;
                err = compiler_get_or_add_def(compiler, packaged_name, &def);
                if (err) return err;

                compiler_binding_t *binding = compiler_get_binding(compiler,
                    name);
                if (binding) {
                    if (!compiler->can_rebind) {
                        fprintf(stderr, "Can't rebind name: %s (%s -> %s)\n",
                            name, binding->name, packaged_name);
                        return 2;
                    }
                    compiler_binding_cleanup(binding);
                } else {
                    ARRAY_PUSH_NEW(compiler_binding_t*, compiler->bindings,
                        new_binding)
                    binding = new_binding;
                }

                binding->name = name;
                binding->def = def;
            }
            GET_CLOSE
        } else {
            return UNEXPECTED("one of: typedef package from");
        }
    }

    return 0;
}

int compiler_compile(compiler_t *compiler, const char *buffer,
    const char *filename
) {
    int err;
    lexer_t *lexer = compiler->lexer;

    err = lexer_load(lexer, buffer, filename);
    if (err) return err;

    err = _compiler_compile(compiler);
    if (err) {
        lexer_info(lexer, stderr);
        fprintf(stderr, "Failed to compile\n");
        compiler_dump(compiler, stderr);
        return err;
    }

    if (!lexer_done(lexer)) {
        return lexer_unexpected(lexer, "end of file");
    }

    lexer_unload(lexer);
    return 0;
}
