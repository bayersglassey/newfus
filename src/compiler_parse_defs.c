

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
    compiler_frame_t *frame, arrayof_inplace_type_field_t *fields,
    bool is_union);



static const char *_build_array_type_name(stringstore_t *store,
    type_ref_t *subtype_ref
) {
    const char *elem_type_name;
    switch (subtype_ref->type.tag) {
        case TYPE_TAG_ARRAY:
            elem_type_name = subtype_ref->type.u.array_f.def->name;
            break;
        case TYPE_TAG_STRUCT: case TYPE_TAG_UNION:
            elem_type_name = subtype_ref->type.u.struct_f.def->name;
            break;
        case TYPE_TAG_ALIAS:
            elem_type_name = subtype_ref->type.u.alias_f.def->name;
            break;
        default:
            elem_type_name = type_tag_sym(subtype_ref->type.tag);
            break;
    }

    const char *array_type_name = elem_type_name;

    if (subtype_ref->is_inplace) {
        array_type_name = _const_strjoin2(store, "inplace_", array_type_name);
        if (!array_type_name) return NULL;
    } else if (subtype_ref->is_weakref) {
        array_type_name = _const_strjoin2(store, "weakref_", array_type_name);
        if (!array_type_name) return NULL;
    }

    array_type_name = _const_strjoin2(store, "arrayof_", array_type_name);
    if (!array_type_name) return NULL;

    return array_type_name;
}

static const char *_build_union_tags_name(stringstore_t *store,
    const char *struct_name
) {
    char *_tags_name = _strjoin2(struct_name, "_tags");
    if (!_tags_name) return NULL;

    _strtoupper(_tags_name);

    const char *tags_name = stringstore_get_donate(store, _tags_name);
    if (!tags_name) {
        free(_tags_name);
        return NULL;
    }

    return tags_name;
}

static const char *_build_field_tag_name(stringstore_t *store,
    const char *union_name, const char *field_name
) {
    char *_tag_name = _strjoin3(union_name, "_tag_", field_name);
    if (!_tag_name) return NULL;

    _strtoupper(_tag_name);

    const char *tag_name = stringstore_get_donate(store, _tag_name);
    if (!tag_name) {
        free(_tag_name);
        return NULL;
    }

    return tag_name;
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

    char *_type_name_upper = _strdup(type_name);
    if (!_type_name_upper) return 1;
    _strtoupper(_type_name_upper);
    const char *type_name_upper = stringstore_get_donate(compiler->store,
        _type_name_upper);
    if (!type_name_upper) {
        free(_type_name_upper);
        return 1;
    }

    /* NOTE: caller guarantees no def exists with name type_name */
    ARRAY_PUSH_NEW(type_def_t*, compiler->defs, def)
    def->name = type_name;
    def->name_upper = type_name_upper;
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

static int compiler_get_or_add_array_def(compiler_t *compiler,
    type_ref_t *subtype_ref, type_def_t **def_ptr
) {
    int err;

    /* NOTE: caller is giving us ownership of subtype_ref.
    We may even free it! So caller should not refer to it anymore.
    Instead, caller may refer to (*def_ptr)->type.u.array_f.subtype_ref
    (which is either the passed subtype_ref, or an equivalent one). */

    const char *array_type_name = _build_array_type_name(compiler->store,
        subtype_ref);
    if (!array_type_name) return 1;

    /* Get or create def */
    type_def_t *def = compiler_get_def(compiler, array_type_name);
    if (def) {
        if (def->type.tag != TYPE_TAG_ARRAY) {
            fprintf(stderr,
                "Def already exists, and is not an array: %s (%s)",
                    array_type_name, type_tag_sym(def->type.tag));
            type_def_t *subdef = type_get_def(&def->type);
            if (subdef) fprintf(stderr, " -> %s", subdef->name);
            fputc('\n', stderr);
            return 2;
        }

        /* TODO: stronger check that def is for the correct array type.
        E.g. by adding a function for comparing references?..
        And then checking e.g that
        type_ref_eq(subtype_ref, def->u.array_f.subtype_ref) */

        /* Free subtype_ref; we don't need it, because it's equivalent to
        def->u.array_f.subtype_ref */
        type_ref_cleanup(subtype_ref);
        free(subtype_ref);
    } else {
        err = compiler_add_def(compiler, array_type_name, &def);
        if (err) return err;

        def->type.tag = TYPE_TAG_ARRAY;
        def->type.u.array_f.def = def;
        def->type.u.array_f.subtype_ref = subtype_ref;
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

    if (GOT("inplace")) {
        NEXT
        ref->is_inplace = true;
    } else if (GOT("weakref")) {
        NEXT
        ref->is_weakref = true;
    }

    err = compiler_parse_type(compiler, frame, &ref->type);
    if (err) return err;

    return 0;
}

static int compiler_parse_type_field(compiler_t *compiler,
    compiler_frame_t *frame, arrayof_inplace_type_field_t *fields,
    bool is_union
) {
    int err;
    lexer_t *lexer = compiler->lexer;

    const char *field_name;
    GET_CONST_NAME(field_name, compiler->store)

    const char *field_type_name = _const_strjoin3(compiler->store, frame->type_name,
        "_", field_name);
    if (!field_type_name) return 1;

    ARRAY_PUSH(type_field_t, *fields, field)
    field->name = field_name;

    if (is_union) {
        const char *tag_name = _build_field_tag_name(compiler->store,
            frame->type_name, field_name);
        if (!tag_name) return 1;
        field->tag_name = tag_name;
    }

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

static int compiler_parse_struct_or_union_def(compiler_t *compiler,
    compiler_frame_t *frame, type_def_t **def_ptr
) {
    int err;
    lexer_t *lexer = compiler->lexer;

    bool is_union = lexer->token[0] == 'u';
    NEXT

    /* NOTE: we may be called in one of two ways... either as a top-level
    "statement" defining a struct, in which case frame == NULL and we require
    that a type_name be syntactically provided (e.g. "struct NAME: ...");
    or as a non-top-level "expression" defining a struct, in which case
    frame != NULL and type_name defaults to frame->type_name. */
    const char *type_name = frame? frame->type_name: NULL;
    if (!frame || GOT_NAME) {
        const char *_type_name;
        GET_CONST_NAME(_type_name, compiler->store)

        type_name = compiler_get_packaged_name(compiler, _type_name);
        if (!type_name) return 1;
    }

    type_def_t *def;
    err = compiler_redef_or_add_def(compiler, type_name, &def);
    if (err) return err;

    memset(&def->type, 0, sizeof(def->type));
    def->type.tag = is_union? TYPE_TAG_UNION: TYPE_TAG_STRUCT;
    def->type.u.struct_f.def = def;
    if (is_union) {
        const char *tags_name = _build_union_tags_name(compiler->store,
            type_name);
        if (!tags_name) return 1;
        def->type.u.struct_f.tags_name = tags_name;
    }

    compiler_frame_t subframe = {0};
    if (frame) subframe = *frame;
    subframe.type_name = type_name;
    GET_OPEN
    while (!DONE && !GOT_CLOSE) {
        err = compiler_parse_type_field(compiler, &subframe,
            &def->type.u.struct_f.fields, is_union);
        if (err) return err;
    }
    GET_CLOSE

    *def_ptr = def;
    return 0;
}

static int compiler_parse_type(compiler_t *compiler,
    compiler_frame_t *frame, type_t *type
) {
    int err;
    lexer_t *lexer = compiler->lexer;

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

        def->is_extern = true;

        /* Type is an alias to def */
        type->tag = TYPE_TAG_ALIAS;
        type->u.alias_f.def = def;
    } else if (GOT("void")) {
        NEXT
        type->tag = TYPE_TAG_VOID;
    } else if (GOT("any")) {
        NEXT
        type->tag = TYPE_TAG_ANY;
    } else if (GOT("type")) {
        NEXT
        type->tag = TYPE_TAG_TYPE;
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

        const char *elem_type_name = frame->type_name;
        if (!frame->array_depth) {
            const char *_elem_type_name = _const_strjoin2(compiler->store,
                elem_type_name, "_elem");
            if (!elem_type_name) return 1;

            elem_type_name = _elem_type_name;
        }

        type_ref_t *subtype_ref = calloc(1, sizeof(*subtype_ref));
        if (!subtype_ref) return 1;

        GET_OPEN
        compiler_frame_t subframe = *frame;
        subframe.array_depth++;
        subframe.type_name = elem_type_name;
        err = compiler_parse_type_ref(compiler, &subframe, subtype_ref);
        if (err) return err;
        GET_CLOSE

        type_def_t *def;
        err = compiler_get_or_add_array_def(compiler, subtype_ref, &def);
        if (err) return err;

        /* Caller gets an *alias* to our array type */
        type->tag = TYPE_TAG_ALIAS;
        type->u.alias_f.def = def;
    } else if (GOT("struct") || GOT("union")) {
        type_def_t *def;
        err = compiler_parse_struct_or_union_def(compiler, frame, &def);
        if (err) return err;

        /* If the type to be returned to caller is different than the type living
        on the def for this struct/union... */
        if (type != &def->type) {
            /* Caller gets an *alias* to our struct/union type */
            type->tag = TYPE_TAG_ALIAS;
            type->u.alias_f.def = def;
        }
    } else {
        return UNEXPECTED(
            "one of: void any int sym bool byte array struct union");
    }

    return 0;
}

int compiler_parse_defs(compiler_t *compiler) {
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
        } else if (GOT("struct") || GOT("union")) {
            type_def_t *def;
            err = compiler_parse_struct_or_union_def(compiler, NULL, &def);
            if (err) return err;
        } else if (GOT("package")) {
            NEXT

            const char *package_name;
            GET_OPEN
            GET_CONST_NAME(package_name, compiler->store)
            GET_CLOSE

            /* The magic name "_" means "no package" */
            if (package_name[0] == '_' && package_name[1] == '\0') {
                package_name = NULL;
            }

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

                def->is_extern = true;

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

    if (!DONE) {
        return UNEXPECTED("end of file");
    }

    return 0;
}

