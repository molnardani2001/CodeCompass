/**
 * Autogenerated by Thrift Compiler (0.13.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Thrift;
using Thrift.Collections;

using Thrift.Protocol;
using Thrift.Protocol.Entities;
using Thrift.Protocol.Utilities;
using Thrift.Transport;
using Thrift.Transport.Client;
using Thrift.Transport.Server;
using Thrift.Processor;



public partial class InvalidInput : TException, TBase
{
  private string _msg;

  public string Msg
  {
    get
    {
      return _msg;
    }
    set
    {
      __isset.msg = true;
      this._msg = value;
    }
  }


  public Isset __isset;
  public struct Isset
  {
    public bool msg;
  }

  public InvalidInput()
  {
  }

  public async Task ReadAsync(TProtocol iprot, CancellationToken cancellationToken)
  {
    iprot.IncrementRecursionDepth();
    try
    {
      TField field;
      await iprot.ReadStructBeginAsync(cancellationToken);
      while (true)
      {
        field = await iprot.ReadFieldBeginAsync(cancellationToken);
        if (field.Type == TType.Stop)
        {
          break;
        }

        switch (field.ID)
        {
          case 1:
            if (field.Type == TType.String)
            {
              Msg = await iprot.ReadStringAsync(cancellationToken);
            }
            else
            {
              await TProtocolUtil.SkipAsync(iprot, field.Type, cancellationToken);
            }
            break;
          default: 
            await TProtocolUtil.SkipAsync(iprot, field.Type, cancellationToken);
            break;
        }

        await iprot.ReadFieldEndAsync(cancellationToken);
      }

      await iprot.ReadStructEndAsync(cancellationToken);
    }
    finally
    {
      iprot.DecrementRecursionDepth();
    }
  }

  public async Task WriteAsync(TProtocol oprot, CancellationToken cancellationToken)
  {
    oprot.IncrementRecursionDepth();
    try
    {
      var struc = new TStruct("InvalidInput");
      await oprot.WriteStructBeginAsync(struc, cancellationToken);
      var field = new TField();
      if (Msg != null && __isset.msg)
      {
        field.Name = "msg";
        field.Type = TType.String;
        field.ID = 1;
        await oprot.WriteFieldBeginAsync(field, cancellationToken);
        await oprot.WriteStringAsync(Msg, cancellationToken);
        await oprot.WriteFieldEndAsync(cancellationToken);
      }
      await oprot.WriteFieldStopAsync(cancellationToken);
      await oprot.WriteStructEndAsync(cancellationToken);
    }
    finally
    {
      oprot.DecrementRecursionDepth();
    }
  }

  public override bool Equals(object that)
  {
    var other = that as InvalidInput;
    if (other == null) return false;
    if (ReferenceEquals(this, other)) return true;
    return ((__isset.msg == other.__isset.msg) && ((!__isset.msg) || (System.Object.Equals(Msg, other.Msg))));
  }

  public override int GetHashCode() {
    int hashcode = 157;
    unchecked {
      if(__isset.msg)
        hashcode = (hashcode * 397) + Msg.GetHashCode();
    }
    return hashcode;
  }

  public override string ToString()
  {
    var sb = new StringBuilder("InvalidInput(");
    bool __first = true;
    if (Msg != null && __isset.msg)
    {
      if(!__first) { sb.Append(", "); }
      __first = false;
      sb.Append("Msg: ");
      sb.Append(Msg);
    }
    sb.Append(")");
    return sb.ToString();
  }
}

