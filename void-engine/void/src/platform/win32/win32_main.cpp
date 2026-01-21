#include "pch.h"
#include "windows.h"
#include "global_persistant_allocator.h"

#include <cstddef>

#include "ds/dynamic_array.h"
#include "ds/flat_hash_map.h"

//belong to game layer
//but platform dependent
#include "application.h"

#ifdef VOID_DEV_MODE

class AVLTree
{
private:
    struct Node
    {
        float value;
        int height;
        Node* left;
        Node* right;
    };
private:
    Node* root = nullptr;

private:
    int height(Node* node)
    {
       if(node == nullptr)
           return 0;

       return node->height;
    }

    Node* rightRotation(Node* current)
    {
        Node* node = current->left;
        current->left = node->right;
        node->right = current;

        current->height = 1 + max(height(current->left), height(current->right));
        node->height = 1 + max(height(node->left), height(node->right));
    
        return node;
    }

    Node* leftRotation(Node* current)
    {
        Node* node = current->right;
        current->right = node->left;
        node->left = current;

        current->height = 1 + max(height(current->left), height(current->right));
        node->height = 1 + max(height(node->left), height(node->right));
    
        return node;   
    }

    Node* insert(Node* current, float value)
    {
        if(current == nullptr)
        {
            current = new Node{value, 1, nullptr, nullptr};
        }
        else
        {
            if(value > current->value)
            {
                current->right = insert(current->right, value);
            }
            else if(value < current->value)
            {
                current->left = insert(current->left, value);
            }
        }

        current->height = 1 + max(height(current->left), height(current->right));
        int balance = height(current->left) - height(current->right);

        //left
        if(balance > 1)
        {
            std::cout << "imbalance at node " << current->value << std::endl;
            
            //left-left
            if(value < current->left->value)
            {
                return rightRotation(current);
            }
            //left-right
            else
            {
                current->left = leftRotation(current->left);
                return rightRotation(current);
            }
        }
        else if(balance < -1)
        {
            std::cout << "imbalance at node " << current->value << std::endl;
            //right-right
            if(value > current->right->value)
            {
                return leftRotation(current);
            }
            //right-left
            else
            {
                current->right = rightRotation(current->right);
                return leftRotation(current);           
            }
        
        }

        return current;
    }

    Node* remove(Node* node, float value)
{
    if(node == nullptr)
        return nullptr;

    // Normal BST delete
    if(value < node->value)
    {
        node->left = remove(node->left, value);
    }
    else if(value > node->value)
    {
        node->right = remove(node->right, value);
    }
    else
    {
        // --- CASE 1: leaf ---
        if(node->left == nullptr && node->right == nullptr)
        {
            delete node;
            return nullptr;
        }

        // --- CASE 2: one child ---
        else if(node->left == nullptr || node->right == nullptr)
        {
            Node* child = node->left ? node->left : node->right;
            delete node;
            return child;
        }

        // --- CASE 3: two children ---
        else
        {
            // find inorder successor (min of right subtree)
            Node* min = node->right;
            while(min->left)
                min = min->left;

            // copy successor value into this node
            node->value = min->value;

            // delete successor in right subtree
            node->right = remove(node->right, min->value);
        }
    }

    // ===== UPDATE HEIGHT =====
    node->height = 1 + max(height(node->left), height(node->right));

    // ===== REBALANCE =====
    int balance = height(node->left) - height(node->right);

    // L > R
    if(balance > 1)
    {
        // LL
        if(height(node->left->left) >= height(node->left->right))
            return rightRotation(node);

        // LR
        node->left = leftRotation(node->left);
        return rightRotation(node);
    }

    // R > L
    if(balance < -1)
    {
        // RR
        if(height(node->right->right) >= height(node->right->left))
            return leftRotation(node);

        // RL
        node->right = rightRotation(node->right);
        return leftRotation(node);
    }

    return node;
}
public:
    AVLTree() = default;

    void insert(float value)
    {
        root = insert(root, value);
    }

    void remove(float value)
    {
        root = remove(root, value);      
    }

    void print()
    {
        std::queue<Node*> q;
        q.push(root);

        std::ostream& os = std::cout;
        while(!q.empty())
        {
            Node* n = q.front();
            q.pop();

            if(n == nullptr)
            {
                os << "null, ";
            }
            else
            {
                q.push(n->left);
                q.push(n->right);

                os << n->value << ", ";
            }

        }

        os << std::endl;
    }
};

class RedBlackTree
{
private:
    struct Node
    {
        bool color;
        float value;
        Node* parent;
        Node* left;
        Node* right;
    };
private:
    Node* root = nullptr;

    void leftRotation(Node* current)
    {
        Node* node = current->right;
        node->parent = current->parent; 
        current->right = node->left;
        
        if(node->left)
            node->left->parent = current;
        
        node->left = current;
        current->parent = node;

        if(!node->parent)
        {
            root = node;
        }
        else
        {
            if(node->parent->left && node->parent->left == current)
            {
                node->parent->left = node;
            }
            else if(node->parent->right && node->parent->right == current)
            {
                node->parent->right = node;
            }
        }
    }
    
    void rightRotation(Node* current)
    {
        Node* node = current->left;
        node->parent = current->parent; 
        current->left = node->right;
        
        if(node->right)
            node->right->parent = current;
        
        node->right = current;
        current->parent = node;  

        if(!node->parent)
        {
            root = node;
        }
        else
        {
            if(node->parent->left && node->parent->left == current)
            {
                node->parent->left = node;
            }
            else if(node->parent->right && node->parent->right == current)
            {
                node->parent->right = node;
            }
        }

    }

    void fixInsert(Node* node)
    {
        while(node != root && node->parent->color)
        {
            Node* grandpa = node->parent->parent;
            //parent in left subtree
            if(node->parent == grandpa->left)
            {
                //red parent - red unc
                if(grandpa->right && grandpa->right->color)
                {
                    node->parent->color = 0;
                    grandpa->right->color = 0;
                    grandpa->color = 1;
                    
                    //propagate to grandparent to continue fixing
                    node = grandpa;
                
                }
                //unc not exist or is black
                else
                {
                    //LR 
                    if(node == node->parent->right)
                    {
                        node = node->parent;
                        leftRotation(node);
                    }

                    //LL
                    node->parent->color = 0;
                    grandpa->color = 1;
                    rightRotation(grandpa);
                }
            }
            //in right subtree
            else
            {
                //red parent - red unc
                if(grandpa->left && grandpa->left->color)
                {
                    node->parent->color = 0;
                    grandpa->left->color = 0;
                    grandpa->color = 1;
                    
                    //propagate to grandparent to continue fixing
                    node = grandpa;
                
                }
                //unc not exist or is black
                else
                {
                    //RL
                    if(node == node->parent->left)
                    {
                        node = node->parent;
                        rightRotation(node);
                    }

                    //RR
                    node->parent->color = 0;
                    grandpa->color = 1;
                    leftRotation(grandpa);
                }            
            }
        }
        root->color = 0;
    }

    void transplant(Node* u, Node* v)
    {
        if(u->parent)
        {
            if(u == u->parent->left)
            {
                u->parent->left = v;
            }
            else
            {
                u->parent->right = v;
            }

            if(v && v->parent)
            {
                v->parent = u->parent;
            }
        }
    }

    void fixDelete(Node* node, Node* nodeParent)
    {
        Node* sibling;
        while(node != root && !Color(node))
        {
            if(node == nodeParent->left)
            {
                sibling = nodeParent->right;
                
                //sibling is red
                if(Color(sibling))
                {
                    sibling->color = 0;
                    nodeParent->color = 1;
                    leftRotation(nodeParent);
                    sibling = nodeParent->right;
                }
                
                if(!sibling || (!Color(sibling->right) && !Color(sibling->left)))
                {
                    if(sibling)
                    {
                        sibling->color = 1;
                    }
                    node = nodeParent;
                }
                // 1 of sibling child is red anyway, so sibling can not be NULL
                else
                {
                    if(!Color(sibling->right))
                    {
                        if(sibling->left)
                        {
                            sibling->left->color = 0;
                        }
                        sibling->color = 1;
                        rightRotation(sibling);
                        sibling = nodeParent->right;
                    }

                    sibling->color = nodeParent->color;
                    nodeParent->color = 0;
                    
                    if(sibling->right)
                    {
                        sibling->right->color = 0;
                    }
                    leftRotation(nodeParent);
                    node = root;
                }
            }
            else
            {
                sibling = nodeParent->left;
                
                //sibling is red
                if(Color(sibling))
                {
                    sibling->color = 0;
                    nodeParent->color = 1;
                    rightRotation(nodeParent);
                    sibling = nodeParent->left;
                }
                
                if(sibling && !Color(sibling->right) && !Color(sibling->left))
                {
                    sibling->color = 1;
                    node = nodeParent;
                }
                else if(sibling)
                {
                    if(!Color(sibling->left))
                    {
                        if(sibling->right)
                        {
                            sibling->right->color = 0;
                        }
                        sibling->color = 1;
                        leftRotation(sibling);
                        sibling = nodeParent->left;
                    }

                    sibling->color = nodeParent->color;
                    nodeParent->color = 0;
                    
                    if(sibling->left)
                    {
                        sibling->left->color = 0;
                    }
                    rightRotation(nodeParent);
                    node = root;
                }            
            }
        }
        
        if(node)
        {
            node->color = 0;
        }
    }

    bool Color(Node* node)
    {
        if(!node)
            return 0;

        return node->color;
    }

public:
    RedBlackTree() = default;

    void insert(float value)
    {
        Node* newNode = new Node{1, value, nullptr, nullptr, nullptr};
        Node* i = root;
        Node* p = nullptr;

        while(i)
        {
            p = i;
            if(value < i->value)
            {
                i = i->left;
            }
            else
            {
                i = i->right;
            }
        }

        newNode->parent = p;

        if(p)
        {
            if(value < p->value)
            {
                p->left = newNode;
            }
            else
            {
                p->right = newNode;
            }
        }
        else
        {
            root = newNode;
        }

        fixInsert(newNode);

    }

    void remove(float value)
    {
        Node* node = root;
        
        while(node)
        {
            if(value < node->value)
            {
                node = node->left;
            }
            else if(value > node->value)
            {
                node = node->right;
            }
            else
            {
                break;
            }
        }
        
        if(!node)
        {
            return;
        }

        bool originalColor;
        Node* x ;
        Node* nodeParent ;
        originalColor = node->color;
        
        if(!node->left)
        {
            x = node->right;
            nodeParent = node->parent;
            transplant(node, node->right);
        }
        else if(!node->right)
        {
            x = node->left;
            nodeParent = node->parent;
            transplant(node, node->left);
        }
        else
        {
            Node* y = node->right;

            while(y->left)
            {
                y = y->left;
            }

            x = y->right;
            originalColor = y->color;

            //y is node->right
            if(y->parent == node)
            {
                nodeParent = y;
            }
            //y inside left subtree of node->right
            else
            {
                nodeParent = y->parent;
                transplant(y, y->right);
                y->right = node->right;
                y->right->parent = y;
            }
            transplant(node, y);
            y->left = node->left;
            y->left->parent = y;
            y->color = node->color;
        }

        delete node;
        //if the node is black, the black height change -> fix the balance
        if(!originalColor)
        {
            fixDelete(x, nodeParent);
        }
    }

    void print()
    {
        std::queue<Node*> q;
        q.push(root);

        std::ostream& os = std::cout;
        while(!q.empty())
        {
            Node* n = q.front();
            q.pop();

            if(n == nullptr)
            {
                os << "NIL, ";
            }
            else
            {
                q.push(n->left);
                q.push(n->right);

                std::string c = n->color? "R" : "B";

                os << n->value << "(" << c <<")" << ", ";
            }

        }

        os << std::endl;
    }
};

int main()
{
    using namespace VoidEngine;
    GlobalPersistantAllocator::SetBufferSize(KB(4));

    Application* app = new Application();

    if(app->StartUp())
    {
        //SIMPLE_LOG(GlobalPersistantAllocator::Get().UsedSize());
        app->Update();
    }
    app->ShutDown();
    delete app;

    return 0;
}

#else

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    using namespace VoidEngine;
    GlobalPersistantAllocator::SetBufferSize(KB(4));
    
    Application* app = new Application();

    if(app->StartUp())
    {
        app->Update();
    }
    app->ShutDown();

    return 0;
}

#endif

