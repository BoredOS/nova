// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_TREE_H
#define NTK_TREE_H
#include "ntk_widget.h"
typedef struct NtkTreeNode NtkTreeNode;
NtkWidget*   ntk_tree_view_new(NtkWidget *parent);
NtkTreeNode* ntk_tree_view_get_root(NtkWidget *tv);
NtkTreeNode* ntk_tree_node_add_child(NtkTreeNode *parent, const char *text);
void         ntk_tree_node_set_text(NtkTreeNode *node, const char *text);
const char*  ntk_tree_node_get_text(NtkTreeNode *node);
void         ntk_tree_node_set_expanded(NtkTreeNode *node, bool expanded);
bool         ntk_tree_node_is_expanded(NtkTreeNode *node);

#endif // NTK_TREE_H
