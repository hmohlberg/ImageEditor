import os

project   = "ImageEditor"
author    = "Forschungszentrum Jülich"
copyright = "2026, Forschungszentrum Jülich"
release   = "1.0"

extensions = [
    "breathe",
    "sphinx.ext.viewcode",
    "sphinx.ext.todo",
]

html_theme = "sphinx_rtd_theme"
html_static_path = []

# ---- Breathe -------------------------------------------------------------
# Path is relative to this conf.py (= docs/).
# Doxygen writes XML to <repo_root>/doxygen_build/xml.
breathe_projects = {
    "ImageEditor": os.path.join(os.path.dirname(__file__), "..", "doxygen_build", "xml")
}
breathe_default_project = "ImageEditor"
breathe_default_members = ("members", "undoc-members")

todo_include_todos = True
