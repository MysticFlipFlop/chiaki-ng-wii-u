# spec-generator — instalación

La skill es una carpeta autocontenida con dos ficheros: `SKILL.md` (las
instrucciones, con frontmatter YAML) y `plantilla-spec.md` (la plantilla).
El contenido es agnóstico de la herramienta; solo cambia dónde se coloca.

## Claude Code

Ya está instalada en este proyecto. Para tenerla en todos tus proyectos:

```bash
cp -R .claude/skills/spec-generator ~/.claude/skills/
```

Se activa sola cuando pides una spec, o a mano con `/spec-generator`.

## opencode

Mismo formato de skill, distinta ruta. Ya está enlazada en este repo
(`.opencode/skill/spec-generator` es un symlink a la carpeta canónica).
En otro proyecto, o si tu sistema no maneja symlinks (Windows sin permisos):

```bash
mkdir -p .opencode/skill && cp -R .claude/skills/spec-generator .opencode/skill/
```

Global en vez de por proyecto: `~/.config/opencode/skill/`.

## Mantenimiento

`SKILL.md` es la única fuente de verdad: opencode lo alcanza por symlink y el
comando de Cursor lo referencia por ruta. Edita solo ese fichero. Si en algún
sitio tienes una copia en vez de un enlace, recuerda sincronizarla.
